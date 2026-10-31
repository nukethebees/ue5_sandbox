#pragma once

#include <ioj/session/level_select_request.h>
#include <ioj/session/telemetry_request.h>

#include <variant>

namespace ioj::session {
using MenuRequest = std::variant<LevelSelectRequest, TelemetryRequest>;
}
