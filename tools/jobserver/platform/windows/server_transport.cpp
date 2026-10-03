#include "transport_common.hpp"

#include <sddl.h>

#include <format>
#include <iostream>

namespace jobserver::platform {
auto serve(std::filesystem::path const& endpoint,
           std::function<ServerReply(std::string const&)> const& handle_request) -> IoResult {
    auto const& sid{windows_detail::user_sid()};
    if (sid.empty()) {
        return IoResult{std::unexpect, "identity_failed", "Cannot determine the local user"};
    }

    auto const acl{std::format(L"D:P(A;;GA;;;{})", sid)};
    PSECURITY_DESCRIPTOR descriptor{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            acl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) {
        return IoResult{
            std::unexpect, "permissions_failed", "Cannot create jobs-board pipe permissions"};
    }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    auto const handle{CreateNamedPipeW(endpoint.c_str(),
                                       PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
                                           PIPE_REJECT_REMOTE_CLIENTS,
                                       1,
                                       65536,
                                       65536,
                                       0,
                                       &security)};
    auto const creation_error{GetLastError()};
    LocalFree(descriptor);
    if (handle == INVALID_HANDLE_VALUE) {
        return IoResult{std::unexpect,
                        "listen_failed",
                        std::format("Cannot create jobs-board pipe: {}", creation_error)};
    }

    windows_detail::Handle const pipe{handle, &CloseHandle};
    bool stopping{};
    while (!stopping) {
        if (!ConnectNamedPipe(pipe.get(), nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            return IoResult{std::unexpect, "accept_failed", "Cannot accept jobs-board request"};
        }

        auto const message{windows_detail::read_message(pipe.get())};
        if (message) {
            auto const reply{handle_request(*message)};
            stopping = reply.stop;
            if (auto result{windows_detail::write_message(pipe.get(), reply.message)}; !result) {
                std::cerr << result.error().message << '\n';
            } else {
                FlushFileBuffers(pipe.get());
            }
        } else {
            std::cerr << message.error().message << '\n';
        }

        DisconnectNamedPipe(pipe.get());
    }
    return {};
}
}
