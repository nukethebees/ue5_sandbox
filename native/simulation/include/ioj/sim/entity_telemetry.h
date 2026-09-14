#pragma once

#include "ioj/sim/entity_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ioj::sim::telemetry {
inline constexpr auto team_count{static_cast<std::size_t>(Team::COUNT)};
inline constexpr auto entity_type_count{static_cast<std::size_t>(EntityType::COUNT)};

using TeamCounts = std::array<std::int32_t, team_count>;
using EntityTypeCounts = std::array<std::int32_t, entity_type_count>;
using EntityCounts = std::array<EntityTypeCounts, team_count>;
using Uint64EntityTypeCounts = std::array<std::uint64_t, entity_type_count>;
using Uint64EntityCounts = std::array<Uint64EntityTypeCounts, team_count>;
using DoubleEntityTypeCounts = std::array<double, entity_type_count>;
using DoubleEntityCounts = std::array<DoubleEntityTypeCounts, team_count>;
using KillMatrix = std::array<std::array<std::uint64_t, team_count>, team_count>;

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
