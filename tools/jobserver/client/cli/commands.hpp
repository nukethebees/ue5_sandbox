#pragma once

#include <nlohmann/json.hpp>

#include <expected>
#include <string>
#include <vector>

namespace jobserver::client::cli {
struct Command {
    nlohmann::json message;
    bool json_output{};
};
struct ParseExit {
    int exit_code;
    std::string message;
};
using ParseResult = std::expected<Command, ParseExit>;

auto parse(std::vector<std::string> const& args) -> ParseResult;
auto format_reply(nlohmann::json const& reply) -> std::string;
}
