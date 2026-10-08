#pragma once

#include <ioj/sim/fighter_entity_data.h>
#include <SpaceGamePresentation/presentation/AgentDisplayBatch.h>

#include <cstdint>
#include <vector>

namespace ml::tests {
struct FDisplayEntityTestData {
    void add_defaulted(std::int32_t const count) {
        motion.add_defaulted(count);
        healths.resize(healths.size() + static_cast<std::size_t>(count));
        entity_ids.resize(entity_ids.size() + static_cast<std::size_t>(count));
        teams.resize(teams.size() + static_cast<std::size_t>(count));
        entity_types.resize(entity_types.size() + static_cast<std::size_t>(count));
    }
    auto num() const noexcept -> std::int32_t { return static_cast<std::int32_t>(healths.size()); }

    void add_health(std::int32_t const row,
                    ::ioj::sim::EntityUniqueId const owner,
                    ::ioj::sim::Health const health) {
        entity_ids[row] = owner;
        healths[row] = health;
    }

    void set_health(std::int32_t const row, ::ioj::sim::Health const health) {
        healths[row] = health;
    }

    ::ioj::sim::FighterEntityData motion;
    std::vector<::ioj::sim::Health> healths;
    std::vector<::ioj::sim::EntityUniqueId> entity_ids;
    std::vector<::ioj::Team> teams;
    std::vector<::ioj::sim::EntityType> entity_types;
};
}
