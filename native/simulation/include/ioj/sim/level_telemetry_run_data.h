#pragma once

#include "ioj/sim/entity_telemetry.h"
#include "ioj/sim/sim_tick.h"
#include "sandbox/core/time_series_data.h"

#include <array>
#include <cstdint>

namespace ioj::sim {
struct LevelTelemetryBattleSample {
    SimTick completed_tick{};
    double simulated_elapsed_seconds{};
    telemetry::CombatTelemetryCounters combat{};
    telemetry::EntityCounts alive{};
    std::int32_t active_lasers{};
    std::int32_t lasers_fired{};
};

struct LevelTelemetryTickSeries {
    static constexpr std::int32_t team_count{static_cast<std::int32_t>(telemetry::team_count)};
    static constexpr std::int32_t entity_type_count{
        static_cast<std::int32_t>(telemetry::entity_type_count)};

    using Int32Data = ml::XYSeriesData<SimTick, std::int32_t>;
    using ActiveEntitiesByTypeData = std::array<Int32Data, entity_type_count>;
    using ActiveEntitiesByTeamAndTypeData = std::array<ActiveEntitiesByTypeData, team_count>;

    Int32Data active_entities{};
    ActiveEntitiesByTypeData active_entities_by_type{};
    ActiveEntitiesByTeamAndTypeData active_entities_by_team_and_type{};
    Int32Data spawned_entities{};
    Int32Data destroyed_entities{};
    Int32Data kills{};
    Int32Data active_lasers{};
    Int32Data lasers_fired{};
};
} // namespace ioj::sim
