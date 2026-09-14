#include "jobserver/transport.hpp"

#include "jobserver/protocol.hpp"

#include <sddl.h>
#include <Windows.h>

#include <array>
#include <cstddef>
#include <vector>

namespace jobserver::transport {
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
        std::wstring result{LR"(\\.\pipe\NukeTheBees.Jobserver.)"};
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

namespace {
auto read_exact(HANDLE const handle, std::byte* data, std::size_t remaining) -> bool {
    while (remaining != 0) {
        DWORD read{};
        auto const chunk{static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD))};
        if (!ReadFile(handle, data, chunk, &read, nullptr) || read == 0) {
            return false;
        }
        data += read;
        remaining -= read;
    }
    return true;
}

auto write_exact(HANDLE const handle, std::byte const* data, std::size_t remaining) -> bool {
    while (remaining != 0) {
        DWORD written{};
        auto const chunk{static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD))};
        if (!WriteFile(handle, data, chunk, &written, nullptr) || written == 0) {
            return false;
        }
        data += written;
        remaining -= written;
    }
    return true;
}
}

auto read_message(void* const native_handle) -> std::expected<std::string, Error> {
    auto const handle{static_cast<HANDLE>(native_handle)};
    std::array<std::byte, 4> header{};
    if (!read_exact(handle, header.data(), header.size())) {
        return std::unexpected(Error{"disconnected", "Scheduler connection closed"});
    }
    auto const decoded_size{protocol::decode_header(header)};
    if (!decoded_size) {
        return std::unexpected(decoded_size.error());
    }
    std::string payload(*decoded_size, '\0');
    if (!payload.empty() &&
        !read_exact(handle, reinterpret_cast<std::byte*>(payload.data()), payload.size())) {
        return std::unexpected(
            Error{"truncated_message", "Scheduler message ended before its payload"});
    }
    return payload;
}

auto write_message(void* const native_handle, std::string const& message)
    -> std::expected<void, Error> {
    auto const frame{protocol::encode_frame(message)};
    if (!frame) {
        return std::unexpected(frame.error());
    }
    if (!write_exact(static_cast<HANDLE>(native_handle), frame->data(), frame->size())) {
        return std::unexpected(
            Error{"write_failed", "Could not write to the scheduler connection"});
    }
    return {};
}
}
