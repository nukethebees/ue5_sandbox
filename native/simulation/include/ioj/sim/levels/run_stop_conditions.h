#pragma once

#include <ioj/duration.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <optional>

namespace ioj::sim {
class EntityLedger;
struct SimClock;

struct RunStopConditions {
    auto stop_reason(SimClock const& clock,
                     EntityLedger const& ledger,
                     bool has_future_spawns) const noexcept
        -> std::optional<LevelTelemetryRunEndReason>;

    bool stop_when_battle_resolved{};
    std::optional<Duration> duration;
};
}
