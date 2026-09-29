#include "commands.hpp"

#include <charconv>
#include <iomanip>

namespace jobserver::cli {
auto parse(std::vector<std::string> const& args) -> std::expected<nlohmann::json, Error> {
    if (args.size() == 3 && args[0] == "request" &&
        (args[1] == "shared" || args[1] == "exclusive") && !args[2].empty()) {
        return nlohmann::json{{"type", "request"}, {"mode", args[1]}, {"name", args[2]}};
    }
    if (args.size() == 2 &&
        (args[0] == "check" || args[0] == "start" || args[0] == "end" || args[0] == "cancel")) {
        TicketId id{};
        auto const* end{args[1].data() + args[1].size()};
        auto const parsed{std::from_chars(args[1].data(), end, id)};
        if (parsed.ec == std::errc{} && parsed.ptr == end && id != 0) {
            return nlohmann::json{{"type", args[0]}, {"id", id}};
        }
    }
    if (args.size() == 1 && (args[0] == "status" || args[0] == "ping" || args[0] == "shutdown")) {
        return nlohmann::json{{"type", args[0]}};
    }
    return std::unexpected(Error{
        "usage", "Use jobs request shared|exclusive NAME, check|start|end|cancel ID, or status"});
}
void print(nlohmann::json const& reply, std::ostream& output) {
    auto ticket = [&](nlohmann::json const& value) {
        output << "  #" << value.at("id").get<TicketId>() << ' ' << std::left << std::setw(10)
               << value.at("mode").get<std::string>() << std::setw(10)
               << value.at("state").get<std::string>() << value.at("name").get<std::string>()
               << '\n';
    };
    if (reply.at("type") == "status") {
        for (auto const* state : {"Running", "Ready", "Queued"}) {
            output << state << '\n';
            bool any{};
            for (auto const& value : reply.at("tickets")) {
                if (value.at("state") == state) {
                    ticket(value);
                    any = true;
                }
            }
            if (!any) {
                output << "  -\n";
            }
        }
    } else if (reply.at("type") == "ticket") {
        ticket(reply);
    } else {
        output << reply.at("type").get<std::string>() << '\n';
    }
}
}
