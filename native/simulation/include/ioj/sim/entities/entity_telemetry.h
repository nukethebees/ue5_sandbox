#pragma once

#include <ioj/sim/entities/team_entity_array.h>
#include <ioj/team_array.h>

#include <cstdint>

namespace ioj::sim::telemetry {
inline constexpr auto team_count{enum_count<Team>()};
inline constexpr auto entity_type_count{enum_count<EntityType>()};

using TeamCounts = TeamArray<std::int32_t>;
using EntityCounts = TeamEntityArray<std::int32_t>;
using Uint64EntityCounts = TeamEntityArray<std::uint64_t>;
using DoubleEntityCounts = TeamEntityArray<double>;

struct CombatTelemetryCounters {
    Uint64EntityCounts spawned{};
    Uint64EntityCounts destroyed{};
    Uint64EntityCounts shots{};
    Uint64EntityCounts hits{};
    DoubleEntityCounts damage_dealt{};
    Uint64EntityCounts kills{};
};
} // namespace ioj::sim::telemetry
