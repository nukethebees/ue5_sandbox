#pragma once
#include "jobserver/error.hpp"
#include "jobserver/platform/transport.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <filesystem>

namespace jobserver::client {
using JsonResult = std::expected<nlohmann::json, Error>;

auto request(nlohmann::json message,
             std::filesystem::path const& endpoint = platform::default_endpoint()) -> JsonResult;
}
