#pragma once

#include <ioj/presentation/agent_display_batch.h>
#include <ioj/sim/fighters/fighter_entity_data.h>

#include <cstdint>
#include <vector>

namespace ioj::tests {
struct FDisplayEntityTestData {
    void add_defaulted(std::int32_t const count) {
        motion.add_defaulted(count);
        healths.resize(healths.size() + static_cast<std::size_t>(count));
        entity_ids.resize(entity_ids.size() + static_cast<std::size_t>(count));
        teams.resize(teams.size() + static_cast<std::size_t>(count));
        entity_types.resize(entity_types.size() + static_cast<std::size_t>(count));
    }
    auto num() const noexcept -> std::int32_t { return static_cast<std::int32_t>(healths.size()); }

    void add_health(std::int32_t const row, sim::EntityId const owner, sim::Health const health) {
        entity_ids[row] = owner;
        healths[row] = health;
    }

    void set_health(std::int32_t const row, sim::Health const health) { healths[row] = health; }

    sim::FighterEntityData motion;
    std::vector<sim::Health> healths;
    std::vector<sim::EntityId> entity_ids;
    std::vector<Team> teams;
    std::vector<sim::EntityType> entity_types;
};
}
