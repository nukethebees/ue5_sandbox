#pragma once

#include <ioj/sim/telemetry/report.h>

#include <expected>
#include <string>
#include <string_view>

namespace ioj::sim {
auto serialize_level_telemetry_report(LevelTelemetryReport const& report)
    -> std::expected<std::string, std::string>;
auto deserialize_level_telemetry_report(std::string_view json)
    -> std::expected<LevelTelemetryReport, std::string>;
}
