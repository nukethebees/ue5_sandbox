#pragma once

#include <expected>
#include <string>

namespace ioj::session {
struct TelemetryRequest {
    std::expected<std::string, std::string> run{};
};
}
