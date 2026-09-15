#include "jobserver/transport.hpp"

#include "jobserver/protocol.hpp"
#include "test_barrier.hpp"

#include <Windows.h>

#include <sddl.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
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
class IoCancellation {
  public:
    explicit IoCancellation(std::stop_token const stop)
        : event_{stop.stop_possible() ? CreateEventW(nullptr, TRUE, FALSE, nullptr) : nullptr}
        , callback_{stop, [handle = event_.handle] { SetEvent(handle); }} {}
    [[nodiscard]] auto handle() const -> HANDLE { return event_.handle; }
  private:
    struct Event {
        HANDLE handle;
        ~Event() {
            if (handle != nullptr) {
                CloseHandle(handle);
            }
        }
    };
    Event event_;
    std::stop_callback<std::function<void()>> callback_;
};

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
                         std::optional<std::chrono::steady_clock::time_point> const deadline,
                         HANDLE const stop_event = nullptr) -> IoResult {
    HANDLE const events[]{overlapped.hEvent, stop_event};
    auto const wait_result{WaitForMultipleObjects(
        stop_event == nullptr ? 1 : 2, events, FALSE, wait_timeout(deadline))};
    if (wait_result == WAIT_TIMEOUT || wait_result == WAIT_OBJECT_0 + 1) {
        CancelIoEx(handle, &overlapped);
        WaitForSingleObject(overlapped.hEvent, INFINITE);
        return wait_result == WAIT_TIMEOUT ? IoResult::timed_out : IoResult::disconnected;
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
                std::optional<std::chrono::steady_clock::time_point> const deadline,
                HANDLE const stop_event = nullptr) -> IoResult {
    while (remaining != 0) {
        if (deadline && wait_timeout(deadline) == 0) {
            return IoResult::timed_out;
        }
        if (stop_event != nullptr && WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0) {
            return IoResult::disconnected;
        }
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
                       ? complete_overlapped(handle, overlapped, read, deadline, stop_event)
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
                 std::optional<std::chrono::steady_clock::time_point> const deadline,
                 HANDLE const stop_event) -> IoResult {
    while (remaining != 0) {
        if (deadline && wait_timeout(deadline) == 0) {
            return IoResult::timed_out;
        }
        if (stop_event != nullptr && WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0) {
            return IoResult::disconnected;
        }
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
            if (error == ERROR_IO_PENDING) {
                test_barrier("during_output_write_pending");
            }
            result = error == ERROR_IO_PENDING
                       ? complete_overlapped(handle, overlapped, written, deadline, stop_event)
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

auto read_message(void* const native_handle,
                  std::optional<std::chrono::milliseconds> const timeout,
                  std::chrono::milliseconds const frame_timeout,
                  std::stop_token const stop) -> std::expected<std::string, Error> {
    auto const handle{static_cast<HANDLE>(native_handle)};
    IoCancellation const cancellation{stop};
    if (stop.stop_possible() && cancellation.handle() == nullptr) {
        return std::unexpected(Error{"read_failed", "Could not create I/O cancellation event"});
    }
    auto deadline{timeout ? std::optional{std::chrono::steady_clock::now() + *timeout}
                          : std::nullopt};
    std::array<std::byte, 4> header{};
    auto header_result{read_exact(handle, header.data(), 1, deadline, cancellation.handle())};
    if (header_result == IoResult::success) {
        auto const frame_deadline{std::chrono::steady_clock::now() + frame_timeout};
        deadline = deadline ? std::min(*deadline, frame_deadline) : frame_deadline;
        header_result = read_exact(
            handle, header.data() + 1, header.size() - 1, deadline, cancellation.handle());
    }
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
    auto const payload_result{payload.empty()
                                  ? IoResult::success
                                  : read_exact(handle,
                                               reinterpret_cast<std::byte*>(payload.data()),
                                               payload.size(),
                                               deadline,
                                               cancellation.handle())};
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
                   std::optional<std::chrono::milliseconds> const timeout,
                   std::stop_token const stop) -> std::expected<void, Error> {
    auto const frame{protocol::encode_frame(message)};
    if (!frame) {
        return std::unexpected(frame.error());
    }
    auto const deadline{timeout ? std::optional{std::chrono::steady_clock::now() + *timeout}
                                : std::nullopt};
    IoCancellation const cancellation{stop};
    if (stop.stop_possible() && cancellation.handle() == nullptr) {
        return std::unexpected(Error{"write_failed", "Could not create I/O cancellation event"});
    }
    auto const result{write_exact(static_cast<HANDLE>(native_handle),
                                  frame->data(),
                                  frame->size(),
                                  deadline,
                                  cancellation.handle())};
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
