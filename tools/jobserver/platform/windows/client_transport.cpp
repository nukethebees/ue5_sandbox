#include "transport_common.hpp"

#include <utility>

namespace jobserver::platform {
auto exchange(std::filesystem::path const& endpoint, std::string const& message) -> MessageResult {
    HANDLE handle{};
    for (;;) {
        handle = CreateFileW(
            endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            break;
        }
        if (GetLastError() != ERROR_PIPE_BUSY || !WaitNamedPipeW(endpoint.c_str(), 5000)) {
            return MessageResult{
                std::unexpect,
                "unavailable",
                "Jobs board unavailable; ask the maintainer to install/start the jobserver"};
        }
    }

    windows_detail::Handle const pipe{handle, &CloseHandle};
    auto sent{windows_detail::write_message(pipe.get(), message)};
    if (!sent) {
        return MessageResult{std::unexpect, std::move(sent).error()};
    }

    return windows_detail::read_message(pipe.get());
}
}
