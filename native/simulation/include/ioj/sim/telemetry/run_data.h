#pragma once

#include <ioj/sim/entities/entity_telemetry.h>
#include <ioj/sim/sim_tick.h>

#include <cstdint>
#include <type_traits>

namespace ioj::sim::telemetry {
// Materialize this dense representation only for consumers and final results.
struct BattleSample {
    SimTick completed_tick{};
    CombatTelemetryCounters combat{};
    EntityCounts alive{};
    std::int32_t active_lasers{};
    std::int32_t lasers_fired{};
};

template <typename Counts>
auto total_count(Counts const& counts) {
    using Value = std::remove_cvref_t<decltype(*counts.begin())>;
    Value total{};
    for (auto const value : counts) {
        total += value;
    }
    return total;
}
}
