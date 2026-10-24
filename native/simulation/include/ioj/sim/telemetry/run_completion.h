#pragma once
#include <ioj/sim/sim_tick.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <string>

namespace ioj::sim {
struct LevelTelemetryRunCompletion {
    LevelTelemetryRunEndReason reason{LevelTelemetryRunEndReason::WorldEnd};
    std::string completed_utc{};
    SimTick completed_ticks{};
};
}
