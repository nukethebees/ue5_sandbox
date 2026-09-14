#pragma once

#include "ioj/sim/level_telemetry_run_data.h"

#include <span>

namespace ioj::sim::telemetry {
[[nodiscard]] auto aggregate_timings(std::span<double> samples) noexcept
    -> LevelTelemetryTimingAggregate;
} // namespace ioj::sim::telemetry
