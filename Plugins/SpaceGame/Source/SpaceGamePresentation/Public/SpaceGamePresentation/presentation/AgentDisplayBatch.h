#pragma once

#include <ioj/sim/column_math.h>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/health_table.h>
#include <ioj/sim/vectors3f.h>

#include <span>

namespace ml::presentation {
using ::ioj::Team;
using ::ioj::sim::EntityType;
using ::ioj::sim::EntityUniqueId;
using ::ioj::sim::Health;
using ::ioj::sim::HealthConstView;
using ::ioj::sim::Vector3f;
using ::ioj::sim::Vectors3fConstView;
// Borrowed owner columns, valid only until the next structural mutation.
// Static/indestructible owners omit columns whose values are constant.
struct AgentDisplayBatch {
    EntityType type{};
    std::span<EntityUniqueId const> ids;
    Vectors3fConstView locations;
    Vectors3fConstView velocities;
    HealthConstView healths;
    std::span<Team const> teams;

    auto num() const noexcept -> std::uint32_t { return locations.num(); }
    auto health(std::uint32_t index) const -> Health {
        return healths.is_empty() ? 1000000 : healths.health(index);
    }
    auto team(std::uint32_t index) const -> Team {
        return teams.empty() ? Team::White : teams[index];
    }
    auto velocity(std::uint32_t index) const -> Vector3f {
        return velocities.num() == 0 ? Vector3f{} : velocities[index];
    }
};
inline auto display_entity_count(std::span<AgentDisplayBatch const> batches) -> std::uint32_t {
    std::uint32_t count{};
    for (auto const& batch : batches) {
        count += batch.num();
    }
    return count;
}
}
