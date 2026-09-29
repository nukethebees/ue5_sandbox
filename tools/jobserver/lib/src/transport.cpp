#include "jobserver/transport.hpp"

#include "jobserver/protocol.hpp"

#include <Windows.h>

#include <sddl.h>

#include <array>
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
                LPWSTR sid{};
                if (ConvertSidToStringSidW(
                        reinterpret_cast<TOKEN_USER const*>(storage.data())->User.Sid, &sid)) {
                    result = sid;
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
    static auto const value{std::wstring{LR"(\\.\pipe\NukeTheBees.Jobserver.)"} + user_sid()};
    return value;
}
namespace io_detail {
auto read_exact(HANDLE handle, void* data, std::size_t size) -> bool {
    auto* next{static_cast<std::byte*>(data)};
    while (size != 0) {
        DWORD count{};
        if (!ReadFile(handle, next, static_cast<DWORD>(size), &count, nullptr) || count == 0) {
            return false;
        }
        next += count;
        size -= count;
    }
    return true;
}
}
auto read_message(void* const handle) -> std::expected<std::string, Error> {
    std::array<std::byte, 4> header{};
    if (!io_detail::read_exact(handle, header.data(), header.size())) {
        return std::unexpected(
            Error{"read_failed", "Jobs-board connection closed before the response"});
    }
    auto const size{protocol::decode_header(header)};
    if (!size) {
        return std::unexpected(size.error());
    }
    std::string message(*size, '\0');
    if (!io_detail::read_exact(handle, message.data(), message.size())) {
        return std::unexpected(Error{"read_failed", "Incomplete jobs-board message"});
    }
    return message;
}
auto write_message(void* const handle, std::string const& message) -> std::expected<void, Error> {
    auto const frame{protocol::encode_frame(message)};
    if (!frame) {
        return std::unexpected(frame.error());
    }
    auto const* next{frame->data()};
    auto remaining{frame->size()};
    while (remaining != 0) {
        DWORD count{};
        if (!WriteFile(handle, next, static_cast<DWORD>(remaining), &count, nullptr) ||
            count == 0) {
            return std::unexpected(Error{"write_failed", "Could not write jobs-board message"});
        }
        next += count;
        remaining -= count;
    }
    return {};
}
}
