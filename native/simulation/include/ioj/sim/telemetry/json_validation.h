#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/sim/telemetry/run_record.h>

#include <cmath>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace ioj::sim::telemetry {

inline constexpr std::uint64_t max_exact_json_integer{9007199254740992ULL};

[[nodiscard]] inline auto is_valid_json_number(double const value, bool const nonnegative) noexcept
    -> bool {
    return nonnegative ? NonNegativeFinite<double>::valid(value) : std::isfinite(value);
}

[[nodiscard]] auto parse_json_int32(double value, std::string_view path)
    -> std::expected<std::int32_t, std::string>;
[[nodiscard]] auto parse_json_uint64(double value, std::string_view path)
    -> std::expected<std::uint64_t, std::string>;
[[nodiscard]] auto validate_level_telemetry_json_record(LevelTelemetryRunRecord const& record)
    -> std::expected<void, std::string>;

} // namespace ioj::sim::telemetry
