#pragma once
#include "jobserver/types.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <ostream>
#include <vector>

namespace jobserver::cli {
auto parse(std::vector<std::string> const& args) -> std::expected<nlohmann::json, Error>;
void print(nlohmann::json const& reply, std::ostream& output);
}
