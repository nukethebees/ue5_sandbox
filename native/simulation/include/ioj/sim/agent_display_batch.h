#pragma once

#include <ioj/sim/entity_types.h>
#include <ioj/sim/health.h>
#include <ioj/sim/vectors3f.h>

#include <span>

namespace ioj::sim {
// Borrowed owner columns, valid only until the next structural mutation.
// Static/indestructible owners omit columns whose values are constant.
struct AgentDisplayBatch {
    EntityType type{};
    std::span<EntityUniqueId const> ids;
    Vectors3fConstView locations;
    Vectors3fConstView velocities;
    std::span<Health const> healths;
    std::span<Team const> teams;

    auto num() const noexcept -> std::int32_t { return locations.num(); }
    auto health(std::int32_t index) const -> Health {
        return healths.empty() ? 1000000 : healths[index];
    }
    auto team(std::int32_t index) const -> Team {
        return teams.empty() ? Team::White : teams[index];
    }
    auto velocity(std::int32_t index) const -> Vector3f {
        return velocities.num() == 0 ? Vector3f{} : velocities[index];
    }
};
inline auto display_entity_count(std::span<AgentDisplayBatch const> batches) -> std::int32_t {
    std::int32_t count{};
    for (auto const& batch : batches) {
        count += batch.num();
    }
    return count;
}
}
