#include "jobserver/transport.hpp"

#include "jobserver/protocol.hpp"

#include <sddl.h>
#include <Windows.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace jobserver::transport {
auto user_sid() -> std::wstring const& {
    static auto const value = [] {
        std::wstring result;
        HANDLE token{};
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            DWORD size{};
            GetTokenInformation(token, TokenUser, nullptr, 0, &size);
            std::vector<std::byte> storage(size);
            if (size != 0 && GetTokenInformation(token, TokenUser, storage.data(), size, &size)) {
                auto const user{reinterpret_cast<TOKEN_USER const*>(storage.data())};
                LPWSTR sid{};
                if (ConvertSidToStringSidW(user->User.Sid, &sid)) {
                    result += sid;
                    LocalFree(sid);
                }
            }
            CloseHandle(token);
        }
        return result;
    }();
    return value;
}

auto pipe_name() -> std::wstring const& {
    static auto const value = [] {
        if (auto const size{
                GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", nullptr, 0)};
            size != 0) {
            std::wstring override_name(size, L'\0');
            GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", override_name.data(), size);
            override_name.resize(size - 1);
            return override_name;
        }
        return std::wstring{LR"(\\.\pipe\NukeTheBees.Jobserver.)"} + user_sid();
    }();
    return value;
}

namespace {
enum class IoResult {
    success,
    disconnected,
    timed_out,
    failed,
};

auto wait_timeout(std::optional<std::chrono::steady_clock::time_point> const deadline) -> DWORD {
    if (!deadline) {
        return INFINITE;
    }
    auto const remaining{std::chrono::duration_cast<std::chrono::milliseconds>(
        *deadline - std::chrono::steady_clock::now())};
    if (remaining <= std::chrono::milliseconds::zero()) {
        return 0;
    }
    return static_cast<DWORD>(std::min<std::int64_t>(remaining.count(), INFINITE - 1ULL));
}

auto complete_overlapped(HANDLE const handle,
                         OVERLAPPED& overlapped,
                         DWORD& transferred,
                         std::optional<std::chrono::steady_clock::time_point> const deadline)
    -> IoResult {
    auto const wait_result{WaitForSingleObject(overlapped.hEvent, wait_timeout(deadline))};
    if (wait_result == WAIT_TIMEOUT) {
        CancelIoEx(handle, &overlapped);
        WaitForSingleObject(overlapped.hEvent, INFINITE);
        return IoResult::timed_out;
    }
    if (wait_result != WAIT_OBJECT_0 ||
        !GetOverlappedResult(handle, &overlapped, &transferred, FALSE)) {
        auto const error{GetLastError()};
        return error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED ||
                       error == ERROR_OPERATION_ABORTED
                 ? IoResult::disconnected
                 : IoResult::failed;
    }
    return transferred == 0 ? IoResult::disconnected : IoResult::success;
}

auto read_exact(HANDLE const handle,
                std::byte* data,
                std::size_t remaining,
                std::optional<std::chrono::steady_clock::time_point> const deadline) -> IoResult {
    while (remaining != 0) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (overlapped.hEvent == nullptr) {
            return IoResult::failed;
        }
        DWORD read{};
        auto const chunk{static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD))};
        auto result{IoResult::success};
        if (!ReadFile(handle, data, chunk, &read, &overlapped)) {
            auto const error{GetLastError()};
            result = error == ERROR_IO_PENDING
                       ? complete_overlapped(handle, overlapped, read, deadline)
                       : (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED
                              ? IoResult::disconnected
                              : IoResult::failed);
        } else if (read == 0) {
            result = IoResult::disconnected;
        }
        CloseHandle(overlapped.hEvent);
        if (result != IoResult::success) {
            return result;
        }
        data += read;
        remaining -= read;
    }
    return IoResult::success;
}

auto write_exact(HANDLE const handle,
                 std::byte const* data,
                 std::size_t remaining,
                 std::optional<std::chrono::steady_clock::time_point> const deadline) -> IoResult {
    while (remaining != 0) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (overlapped.hEvent == nullptr) {
            return IoResult::failed;
        }
        DWORD written{};
        auto const chunk{static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD))};
        auto result{IoResult::success};
        if (!WriteFile(handle, data, chunk, &written, &overlapped)) {
            auto const error{GetLastError()};
            result = error == ERROR_IO_PENDING
                       ? complete_overlapped(handle, overlapped, written, deadline)
                       : (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED
                              ? IoResult::disconnected
                              : IoResult::failed);
        } else if (written == 0) {
            result = IoResult::disconnected;
        }
        CloseHandle(overlapped.hEvent);
        if (result != IoResult::success) {
            return result;
        }
        data += written;
        remaining -= written;
    }
    return IoResult::success;
}
}

auto read_message(void* const native_handle, std::optional<std::chrono::milliseconds> const timeout)
    -> std::expected<std::string, Error> {
    auto const handle{static_cast<HANDLE>(native_handle)};
    auto const deadline{timeout ? std::optional{std::chrono::steady_clock::now() + *timeout}
                                : std::nullopt};
    std::array<std::byte, 4> header{};
    auto const header_result{read_exact(handle, header.data(), header.size(), deadline)};
    if (header_result == IoResult::timed_out) {
        return std::unexpected(Error{"read_timeout", "Scheduler read timed out"});
    }
    if (header_result != IoResult::success) {
        return std::unexpected(Error{"disconnected", "Scheduler connection closed"});
    }
    auto const decoded_size{protocol::decode_header(header)};
    if (!decoded_size) {
        return std::unexpected(decoded_size.error());
    }
    std::string payload(*decoded_size, '\0');
    auto const payload_result{
        payload.empty()
            ? IoResult::success
            : read_exact(
                  handle, reinterpret_cast<std::byte*>(payload.data()), payload.size(), deadline)};
    if (payload_result == IoResult::timed_out) {
        return std::unexpected(Error{"read_timeout", "Scheduler read timed out"});
    }
    if (payload_result != IoResult::success) {
        return std::unexpected(
            Error{"truncated_message", "Scheduler message ended before its payload"});
    }
    return payload;
}

auto write_message(void* const native_handle,
                   std::string const& message,
                   std::optional<std::chrono::milliseconds> const timeout)
    -> std::expected<void, Error> {
    auto const frame{protocol::encode_frame(message)};
    if (!frame) {
        return std::unexpected(frame.error());
    }
    auto const deadline{timeout ? std::optional{std::chrono::steady_clock::now() + *timeout}
                                : std::nullopt};
    auto const result{
        write_exact(static_cast<HANDLE>(native_handle), frame->data(), frame->size(), deadline)};
    if (result == IoResult::timed_out) {
        return std::unexpected(Error{"write_timeout", "Scheduler write timed out"});
    }
    if (result != IoResult::success) {
        return std::unexpected(
            Error{"write_failed", "Could not write to the scheduler connection"});
    }
    return {};
}
}
