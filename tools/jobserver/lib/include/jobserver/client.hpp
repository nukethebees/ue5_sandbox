#pragma once
#include "jobserver/transport.hpp"

#include <nlohmann/json.hpp>

namespace jobserver {
auto request(nlohmann::json message, std::wstring const& endpoint = transport::pipe_name())
    -> std::expected<nlohmann::json, Error>;
}
