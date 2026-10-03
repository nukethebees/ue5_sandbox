#include "commands.hpp"

#include "jobserver/protocol.hpp"
#include "jobserver/ticket_id.hpp"

#include <CLI/CLI.hpp>

#include <charconv>
#include <format>
#include <iterator>
#include <sstream>
#include <utility>

namespace jobserver::client::cli {
auto parse(std::vector<std::string> const& args) -> ParseResult {
    ParseResult result{std::in_place};
    auto& command{*result};
    std::string mode{};
    std::string name{};
    TicketId id{};

    CLI::App app{"Coordinate heavyweight work with the per-user jobs board.", "jobserver"};
    app.get_formatter()->enable_footer_formatting(false);
    app.require_subcommand(1, 1);
    app.set_help_all_flag("--help-all", "Show help for every command");
    app.set_version_flag("--version",
                         std::format("jobserver 0.4.0 (protocol {})", protocol::major_version));
    app.add_flag("--json", command.json_output, "Print the successful server response as JSON");
    app.fallthrough();

    auto* request{
        app.add_subcommand("request", "Request a shared or exclusive ticket; return immediately")};
    request->group("Ticket commands");
    request
        ->add_option(
            "MODE", mode, "shared: builds/tests; exclusive: benchmarks or quiet-machine work")
        ->required()
        ->check(CLI::IsMember({"shared", "exclusive"}));
    request->add_option("NAME", name, "Descriptive work name; quote names containing spaces")
        ->required()
        ->check(CLI::Validator{[](std::string const& value) {
                                   return value.empty() ? "A descriptive name is required" : "";
                               },
                               "NONEMPTY"});
    request->footer(R"(Examples:
  jobserver request shared "build tools"
  jobserver request exclusive "benchmark" --json)");
    request->callback(
        [&] { command.message = {{"type", "request"}, {"mode", mode}, {"name", name}}; });

    auto const ticket_id{CLI::Validator{
        [](std::string& value) -> std::string {
            TicketId parsed_id{};
            auto const* end{value.data() + value.size()};
            auto const parsed{std::from_chars(value.data(), end, parsed_id)};
            if (parsed.ec != std::errc{} || parsed.ptr != end || parsed_id == 0) {
                return "Expected a positive decimal ticket ID (1..18446744073709551615)";
            }

            // Preserve decimal IDs with leading zeros before CLI11's integer conversion.
            value = std::to_string(parsed_id);
            return {};
        },
        "POSITIVE"}};
    auto add_ticket_command = [&](char const* verb, char const* description) {
        auto* subcommand{app.add_subcommand(verb, description)};
        subcommand->group("Ticket commands");
        subcommand->add_option("ID", id, "Active ticket ID returned by request")
            ->required()
            ->transform(ticket_id);
        subcommand->callback([&, verb] { command.message = {{"type", verb}, {"id", id}}; });
    };
    add_ticket_command("check", "Show one active ticket and its current state");
    add_ticket_command("start", "Mark a Ready ticket as Running immediately before work");
    add_ticket_command("end", "Finish a Running ticket and release its place on the board");
    add_ticket_command("cancel", "Remove an unused Queued or Ready ticket");

    auto* status{app.add_subcommand("status", "Show all active tickets grouped by state")};
    status->group("Ticket commands");
    status->footer(R"(Examples:
  jobserver status
  jobserver status --json)");
    status->callback([&] { command.message = {{"type", "status"}}; });

    auto* ping{app.add_subcommand("ping", "Check that the daemon is reachable")};
    ping->group("Daemon administration");
    ping->callback([&] { command.message = {{"type", "ping"}}; });

    auto* shutdown{app.add_subcommand("shutdown", "Stop the daemon only when the board is empty")};
    shutdown->group("Daemon administration");
    shutdown->callback([&] { command.message = {{"type", "shutdown"}}; });

    app.footer(R"(Examples:
  jobserver request shared "build tools"
  jobserver status --json

Use jobserver <command> --help for command options.
Documentation: tools/jobserver/docs/client.md and tools/jobserver/docs/server.md)");

    if (args.empty()) {
        return ParseResult{std::unexpect, 0, app.help()};
    }

    try {
        // Supply the reversed argument order expected by CLI11's vector overload.
        auto arguments{std::vector<std::string>{args.rbegin(), args.rend()}};
        app.parse(arguments);
    } catch (CLI::ParseError const& error) {
        // Capture CLI11's stream-only exit output.
        std::ostringstream output;
        auto const exit_code{app.exit(error, output, output)};
        return ParseResult{std::unexpect, exit_code, std::move(output).str()};
    }

    return result;
}
auto format_reply(nlohmann::json const& reply) -> std::string {
    std::string output{};
    auto ticket = [&](nlohmann::json const& value) {
        std::format_to(std::back_inserter(output),
                       "  #{} {:<10}{:<10}{}\n",
                       value.at("id").get<TicketId>(),
                       value.at("mode").get_ref<std::string const&>(),
                       value.at("state").get_ref<std::string const&>(),
                       value.at("name").get_ref<std::string const&>());
    };
    if (reply.at("type") == "status") {
        for (auto const* state : {"Running", "Ready", "Queued"}) {
            std::format_to(std::back_inserter(output), "{}\n", state);
            bool any{};
            for (auto const& value : reply.at("tickets")) {
                if (value.at("state") == state) {
                    ticket(value);
                    any = true;
                }
            }
            if (!any) {
                output += "  -\n";
            }
        }
    } else if (reply.at("type") == "ticket") {
        ticket(reply);
    } else {
        std::format_to(
            std::back_inserter(output), "{}\n", reply.at("type").get_ref<std::string const&>());
    }
    return output;
}
}
