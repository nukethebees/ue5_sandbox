#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/team.h>
#include <ioj/sim/entities/entity_type.h>

#include <cstdint>

namespace ioj::sim::telemetry {
inline constexpr auto team_count{enum_count<Team>()};
inline constexpr auto entity_type_count{enum_count<EntityType>()};

using TeamCounts = EnumArray<Team, std::int32_t>;
using EntityTypeCounts = EnumArray<EntityType, std::int32_t>;
using EntityCounts = EnumArray<Team, EntityTypeCounts>;
using Uint64EntityTypeCounts = EnumArray<EntityType, std::uint64_t>;
using Uint64EntityCounts = EnumArray<Team, Uint64EntityTypeCounts>;
using DoubleEntityTypeCounts = EnumArray<EntityType, double>;
using DoubleEntityCounts = EnumArray<Team, DoubleEntityTypeCounts>;
using KillMatrix = EnumArray<Team, EnumArray<Team, std::uint64_t>>;

struct CombatTelemetryCounters {
    Uint64EntityCounts spawned{};
    Uint64EntityCounts destroyed{};
    Uint64EntityCounts shots{};
    Uint64EntityCounts hits{};
    DoubleEntityCounts damage_dealt{};
    DoubleEntityCounts damage_received{};
    Uint64EntityCounts kills{};
    Uint64EntityCounts losses{};
    KillMatrix kill_matrix{};
};
} // namespace ioj::sim::telemetry
