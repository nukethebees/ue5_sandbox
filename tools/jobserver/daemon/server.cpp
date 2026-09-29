#include "server.hpp"

#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <sddl.h>

#include <iostream>

namespace jobserver {
Server::Server(std::wstring endpoint)
    : endpoint_{std::move(endpoint)} {}
auto Server::run() -> int {
    auto const& sid{transport::user_sid()};
    if (sid.empty()) {
        std::cerr << "Cannot determine the local user\n";
        return 1;
    }
    auto const acl{L"D:P(A;;GA;;;" + sid + L")"};
    PSECURITY_DESCRIPTOR descriptor{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            acl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) {
        std::cerr << "Cannot create jobs-board pipe permissions\n";
        return 1;
    }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    auto const pipe{CreateNamedPipeW(endpoint_.c_str(),
                                     PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                     PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
                                         PIPE_REJECT_REMOTE_CLIENTS,
                                     1,
                                     65536,
                                     65536,
                                     0,
                                     &security)};
    LocalFree(descriptor);
    if (pipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Cannot create jobs-board pipe: " << GetLastError() << '\n';
        return 1;
    }

    while (!stopping_) {
        if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            std::cerr << "Cannot accept jobs-board request: " << GetLastError() << '\n';
            CloseHandle(pipe);
            return 1;
        }
        auto const text{transport::read_message(pipe)};
        if (text) {
            nlohmann::json reply;
            try {
                reply = request(nlohmann::json::parse(*text));
            } catch (std::exception const& error) {
                reply = {{"type", "error"}, {"code", "invalid_request"}, {"message", error.what()}};
            }
            if (auto result{transport::write_message(pipe, reply.dump())}; !result) {
                std::cerr << result.error().message << '\n';
            } else {
                FlushFileBuffers(pipe);
            }
        } else {
            std::cerr << text.error().message << '\n';
        }
        DisconnectNamedPipe(pipe);
    }
    CloseHandle(pipe);
    return 0;
}
auto Server::request(nlohmann::json const& message) -> nlohmann::json {
    auto error = [](std::string const& code, std::string const& reason) {
        return nlohmann::json{{"type", "error"}, {"code", code}, {"message", reason}};
    };
    if (message.value("protocol", 0U) != protocol::major_version) {
        return error("protocol_mismatch", "Install matching AgentTask/jobserver components");
    }
    auto const type{message.at("type").get<std::string>()};
    if (type == "status") {
        return board_.status();
    }
    if (type == "ping") {
        return {{"type", "pong"}};
    }
    if (type == "shutdown") {
        if (!board_.empty()) {
            return error("board_busy",
                         "End running tickets and cancel queued/ready tickets before shutdown");
        }
        stopping_ = true;
        return {{"type", "accepted"}};
    }

    std::expected<Ticket, Error> result;
    if (type == "request") {
        auto const mode{message.at("mode").get<std::string>()};
        if (mode != "shared" && mode != "exclusive") {
            return error("invalid_mode", "Choose shared or exclusive");
        }
        result = board_.request(mode == "shared" ? Mode::shared : Mode::exclusive,
                                message.at("name").get<std::string>());
    } else if (type == "check" || type == "start" || type == "end" || type == "cancel") {
        auto const& value{message.at("id")};
        if (!value.is_number_unsigned()) {
            return error("invalid_ticket", "Ticket ID must be a positive integer");
        }
        auto const id{value.get<TicketId>()};
        result = type == "check" ? board_.check(id)
                                 : board_.transition(id,
                                                     type == "start" ? State::running
                                                     : type == "end" ? State::done
                                                                     : State::cancelled);
    } else {
        return error("unknown_message", "Unknown jobs-board request");
    }
    if (!result) {
        return error(result.error().code, result.error().message);
    }
    auto reply = ticket_json(*result);
    reply["type"] = "ticket";
    return reply;
}
}
