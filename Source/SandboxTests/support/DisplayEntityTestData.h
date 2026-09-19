#pragma once

#include <ioj/sim/agent_display_batch.h>
#include <ioj/sim/vectors3f.h>

#include <cstdint>
#include <vector>

namespace ml::tests {
struct FDisplayEntityTestData {
    void add_defaulted(std::int32_t const count) {
        locations.add_defaulted(count);
        velocities.add_defaulted(count);
        health_indices.resize(health_indices.size() + static_cast<std::size_t>(count));
        entity_ids.resize(entity_ids.size() + static_cast<std::size_t>(count));
        teams.resize(teams.size() + static_cast<std::size_t>(count));
        entity_types.resize(entity_types.size() + static_cast<std::size_t>(count));
    }
    auto num() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(health_indices.size());
    }

    void add_health(std::int32_t const row,
                    ::ioj::sim::EntityUniqueId const owner,
                    ::ioj::sim::Health const health) {
        entity_ids[row] = owner;
        health_table.add(std::span<::ioj::sim::EntityUniqueId const>{&owner, 1},
                         health,
                         std::span<::ioj::sim::HealthIndex>{&health_indices[row], 1});
    }

    void set_health(std::int32_t const row, ::ioj::sim::Health const health) {
        health_table.get_view(health_indices, entity_ids).health(row) = health;
    }

    ::ioj::sim::Vectors3f locations;
    ::ioj::sim::Vectors3f velocities;
    ::ioj::sim::HealthTable health_table;
    std::vector<::ioj::sim::HealthIndex> health_indices;
    std::vector<::ioj::sim::EntityUniqueId> entity_ids;
    std::vector<::ioj::sim::Team> teams;
    std::vector<::ioj::sim::EntityType> entity_types;
};
}
