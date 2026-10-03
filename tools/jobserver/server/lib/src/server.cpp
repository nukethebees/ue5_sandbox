#include "jobserver/server/server.hpp"

#include "jobserver/platform/transport.hpp"
#include "jobserver/protocol.hpp"

#include <iostream>
#include <utility>

namespace jobserver::server {
Server::Server(std::filesystem::path endpoint)
    : endpoint_{std::move(endpoint)} {}
auto Server::run() -> int {
    auto const result{platform::serve(endpoint_, [this](std::string const& message) {
        nlohmann::json reply;
        try {
            reply = request(nlohmann::json::parse(message));
        } catch (std::exception const& error) {
            reply = {{"type", "error"}, {"code", "invalid_request"}, {"message", error.what()}};
        }
        return platform::ServerReply{reply.dump(), stopping_};
    })};
    if (!result) {
        std::cerr << result.error().message << '\n';
        return 1;
    }
    return 0;
}
auto Server::request(nlohmann::json const& message) -> nlohmann::json {
    auto error = [](std::string const& code, std::string const& reason) {
        return nlohmann::json{{"type", "error"}, {"code", code}, {"message", reason}};
    };
    if (message.value("protocol", 0U) != protocol::major_version) {
        return error("protocol_mismatch", "Install matching coj/jobserver components");
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

    TicketResult result;
    if (type == "request") {
        auto const mode{message.at("mode").get<std::string>()};
        if (mode != "shared" && mode != "exclusive") {
            return error("invalid_mode", "Choose shared or exclusive");
        }
        result = board_.request(mode == "shared" ? Mode::shared : Mode::exclusive,
                                message.at("name").get<std::string>(),
                                message.at("owner").get<std::string>(),
                                message.at("worktree").get<std::string>());
    } else if (type == "clear" && message.contains("owner")) {
        if (message.contains("id")) {
            return error("invalid_selector", "Clear requires either a ticket ID or an owner");
        }
        auto const owner{message.at("owner").get<std::string>()};
        if (owner.empty()) {
            return error("invalid_owner", "A nonempty owner is required");
        }
        std::optional<std::string> worktree;
        if (message.contains("worktree")) {
            worktree = message.at("worktree").get<std::string>();
        }
        return {{"type", "cleared"}, {"tickets", board_.clear_owner(owner, worktree)}};
    } else if (type == "check" || type == "start" || type == "end" || type == "cancel" ||
               type == "clear") {
        auto const& value{message.at("id")};
        if (!value.is_number_unsigned() || value.get<TicketId>() == 0) {
            return error("invalid_ticket", "Ticket ID must be a positive integer");
        }
        auto const id{value.get<TicketId>()};
        if (type == "clear") {
            if (message.contains("worktree")) {
                return error("invalid_selector", "Worktree filtering requires an owner");
            }
            result = board_.clear(id);
            if (!result) {
                return error(result.error().code, result.error().message);
            }
            return {{"type", "cleared"},
                    {"tickets", nlohmann::json::array({ticket_json(*result)})}};
        }
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
