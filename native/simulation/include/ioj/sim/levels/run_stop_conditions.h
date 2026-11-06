#pragma once

#include <ioj/duration.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <optional>

namespace ioj::sim {
struct SimClock;

struct RunStopConditions {
    auto stop_reason(SimClock const& clock) const noexcept
        -> std::optional<telemetry::RunEndReason>;

    std::optional<Duration> duration;
};
}
