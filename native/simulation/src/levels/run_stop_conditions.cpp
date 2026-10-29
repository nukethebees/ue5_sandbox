#include <ioj/sim/levels/run_stop_conditions.h>

#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/sim_clock.h>

#include <cstdint>

namespace ioj::sim {
auto RunStopConditions::stop_reason(SimClock const& clock,
                                    EntityLedger const& ledger,
                                    bool const has_future_spawns) const noexcept
    -> std::optional<LevelTelemetryRunEndReason> {
    if (stop_when_battle_resolved && !has_future_spawns) {
        auto const alive{ledger.count_alive_per_team()};
        std::int32_t living_teams{};
        for (auto const count : alive) {
            living_teams += count > 0;
        }
        if (living_teams <= 1) {
            return LevelTelemetryRunEndReason::BattleResolved;
        }
    }

    if (duration && clock.get_simulation_time() >= duration->get_seconds()) {
        return LevelTelemetryRunEndReason::DurationReached;
    }
    return std::nullopt;
}
}
