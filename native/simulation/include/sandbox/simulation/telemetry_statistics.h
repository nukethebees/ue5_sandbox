#pragma once

#include "sandbox/simulation/level_telemetry_run_data.h"

#include <span>

namespace ml::simulation::telemetry {
[[nodiscard]] auto aggregate_timings(std::span<double> samples) noexcept
    -> LevelTelemetryTimingAggregate;
} // namespace ml::simulation::telemetry
