#include <ioj/sim/levels/run_stop_conditions.h>

#include <ioj/sim/sim_clock.h>

namespace ioj::sim {
auto RunStopConditions::stop_reason(SimClock const& clock) const noexcept
    -> std::optional<telemetry::RunEndReason> {
    if (duration && clock.simulation_time() >= duration->seconds()) {
        return telemetry::RunEndReason::DurationReached;
    }
    return std::nullopt;
}
}
