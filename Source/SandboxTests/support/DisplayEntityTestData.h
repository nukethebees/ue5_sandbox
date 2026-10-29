#pragma once

#include <ioj/presentation/agent_display_batch.h>
#include <ioj/sim/fighters/fighter_entity_data.h>

#include <cstdint>
#include <span>
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

    void add_health(std::int32_t const row,
                    sim::EntityId const owner,
                    sim::Health::value_type const health) {
        entity_ids[row] = owner;
        healths[row] = sim::Health{health};
    }

    void set_health(std::int32_t const row, sim::Health::value_type const health) {
        healths[row] = sim::Health{health};
    }

    auto make_batches(std::span<sim::EntityId const> ids = {}) const
        -> std::vector<presentation::AgentDisplayBatch> {
        std::vector<presentation::AgentDisplayBatch> batches;
        auto const count{num()};
        batches.reserve(count);
        for (std::int32_t i{}; i < count; ++i) {
            auto const row{motion.get_const_view(i, 1)};
            batches.push_back({entity_types[static_cast<std::size_t>(i)],
                               ids.empty() ? ids : ids.subspan(i, 1),
                               row.view_locations(),
                               row.view_velocities(),
                               sim::HealthConstView{std::span{healths}.subspan(i, 1)},
                               std::span{teams}.subspan(i, 1)});
        }
        return batches;
    }

    sim::FighterEntityData motion;
    std::vector<sim::Health> healths;
    std::vector<sim::EntityId> entity_ids;
    std::vector<Team> teams;
    std::vector<sim::EntityType> entity_types;
};
}
