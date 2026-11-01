#pragma once
#include <ioj/sim/sim_tick.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <string>

namespace ioj::sim::telemetry {
struct RunCompletion {
    RunEndReason reason{RunEndReason::WorldEnd};
    std::string completed_utc{};
    SimTick completed_ticks{};
};
}
