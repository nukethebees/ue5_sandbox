#pragma once

#include "ioj/sim/entities/entity_telemetry.h"
#include "ioj/sim/sim_tick.h"
#include "ioj/time_series_data.h"

#include <cstdint>

namespace ioj::sim::telemetry {
struct BattleSample {
    SimTick completed_tick{};
    CombatTelemetryCounters combat{};
    EntityCounts alive{};
    std::int32_t active_lasers{};
    std::int32_t lasers_fired{};
};

struct TickSeries {
    static constexpr std::int32_t team_count{static_cast<std::int32_t>(telemetry::team_count)};
    static constexpr std::int32_t entity_type_count{
        static_cast<std::int32_t>(telemetry::entity_type_count)};

    using Int32Data = XYSeriesData<SimTick::value_type, std::int32_t>;
    using ActiveEntitiesByTypeData = EnumArray<EntityType, Int32Data>;
    using ActiveEntitiesByTeamAndTypeData = EnumArray<Team, ActiveEntitiesByTypeData>;

    Int32Data active_entities{};
    ActiveEntitiesByTypeData active_entities_by_type{};
    ActiveEntitiesByTeamAndTypeData active_entities_by_team_and_type{};
    Int32Data spawned_entities{};
    Int32Data destroyed_entities{};
    Int32Data kills{};
    Int32Data active_lasers{};
    Int32Data lasers_fired{};
};
} // namespace ioj::sim::telemetry
