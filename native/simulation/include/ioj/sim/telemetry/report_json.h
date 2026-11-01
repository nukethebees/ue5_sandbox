#pragma once

#include <ioj/sim/telemetry/report.h>

#include <expected>
#include <string>
#include <string_view>

namespace ioj::sim::telemetry {
auto serialize_report(Report const& report) -> std::expected<std::string, std::string>;
auto deserialize_report(std::string_view json) -> std::expected<Report, std::string>;
}
