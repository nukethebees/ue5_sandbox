#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/team_array.h>

#include <cstdint>

namespace ioj::sim::telemetry {
inline constexpr auto team_count{enum_count<Team>()};
inline constexpr auto entity_type_count{enum_count<EntityType>()};

using TeamCounts = TeamArray<std::int32_t>;
using EntityTypeCounts = EnumArray<EntityType, std::int32_t>;
using EntityCounts = TeamArray<EntityTypeCounts>;
using Uint64EntityTypeCounts = EnumArray<EntityType, std::uint64_t>;
using Uint64EntityCounts = TeamArray<Uint64EntityTypeCounts>;
using DoubleEntityTypeCounts = EnumArray<EntityType, double>;
using DoubleEntityCounts = TeamArray<DoubleEntityTypeCounts>;

struct CombatTelemetryCounters {
    Uint64EntityCounts spawned{};
    Uint64EntityCounts destroyed{};
    Uint64EntityCounts shots{};
    Uint64EntityCounts hits{};
    DoubleEntityCounts damage_dealt{};
    Uint64EntityCounts kills{};
};
} // namespace ioj::sim::telemetry
