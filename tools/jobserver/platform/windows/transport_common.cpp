#include "transport_common.hpp"

#include "jobserver/protocol.hpp"

#include <sddl.h>

#include <array>
#include <format>
#include <utility>
#include <vector>

namespace jobserver::platform {
namespace windows_detail {
auto user_sid() -> std::wstring const& {
    static auto const value = [] {
        std::wstring result;
        HANDLE token{};
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            Handle const owner{token, &CloseHandle};
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
        }
        return result;
    }();
    return value;
}
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
auto read_message(HANDLE const handle) -> MessageResult {
    std::array<std::byte, protocol::header_size> header{};
    if (!read_exact(handle, header.data(), header.size())) {
        return MessageResult{
            std::unexpect, "read_failed", "Jobs-board connection closed before the response"};
    }
    auto size{protocol::decode_header(header)};
    if (!size) {
        return MessageResult{std::unexpect, std::move(size).error()};
    }
    MessageResult result{std::in_place, *size, '\0'};
    auto& message{*result};
    if (!read_exact(handle, message.data(), message.size())) {
        return MessageResult{std::unexpect, "read_failed", "Incomplete jobs-board message"};
    }
    return result;
}
auto write_message(HANDLE const handle, std::string const& message) -> IoResult {
    auto frame{protocol::encode_frame(message)};
    if (!frame) {
        return IoResult{std::unexpect, std::move(frame).error()};
    }
    auto const* next{frame->data()};
    auto remaining{frame->size()};
    while (remaining != 0) {
        DWORD count{};
        if (!WriteFile(handle, next, static_cast<DWORD>(remaining), &count, nullptr) ||
            count == 0) {
            return IoResult{std::unexpect, "write_failed", "Could not write jobs-board message"};
        }
        next += count;
        remaining -= count;
    }
    return {};
}
}

auto default_endpoint() -> std::filesystem::path const& {
    static std::filesystem::path const value{
        std::format(LR"(\\.\pipe\NukeTheBees.Jobserver.{})", windows_detail::user_sid())};
    return value;
}
}
