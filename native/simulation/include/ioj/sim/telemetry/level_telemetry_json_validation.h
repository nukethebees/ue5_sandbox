#pragma once

#include <ioj/sim/telemetry/level_telemetry_run_record.h>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace ioj::sim::telemetry {

[[nodiscard]] auto parse_json_int32(double value, std::string_view path)
    -> std::expected<std::int32_t, std::string>;
[[nodiscard]] auto parse_json_uint64(double value, std::string_view path)
    -> std::expected<std::uint64_t, std::string>;
[[nodiscard]] auto validate_level_telemetry_json_record(LevelTelemetryRunRecord const& record)
    -> std::expected<void, std::string>;

} // namespace ioj::sim::telemetry
