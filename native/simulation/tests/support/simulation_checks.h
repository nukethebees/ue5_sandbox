#pragma once

#include <ioj/frame_memory_resource.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/testing/entity_observations.h>

#include <gtest/gtest.h>

#include <vector>

namespace ioj::sim::tests {
inline void expect_frame_reclaimed(FrameMemoryStats const& stats) {
    EXPECT_EQ(stats.outstanding_allocation_count, 0u);
    EXPECT_EQ(stats.current_claimed_bytes, 0u);
}

inline void expect_entity_unregistered(LevelSim const& simulation, EntityId const id) {
    SCOPED_TRACE(::testing::PrintToString(id));
    EXPECT_EQ(observe_entity_row(simulation, id), EntityInstanceHandle::invalid_value);
    EXPECT_FALSE(observe_entity(simulation, id));
    EXPECT_FALSE(observe_live_entity(simulation, id));
}

inline void expect_membership(LevelSim const& simulation) {
    auto const& capitals{simulation.get_capital_ships()};
    auto const& fighters{simulation.get_fighters()};
    auto const entities{fighters.get_entities()};
    auto const capital_entities{capitals.get_entities()};
    auto const capital_ids{capital_entities.entity_ids()};
    auto const spans{capital_entities.fighter_id_spans()};
    auto const parent_ids{entities.parent_ids()};
    auto const fighter_ids{entities.entity_ids()};
    auto const healths{fighters.get_healths()};
    auto const capital_count{capital_entities.num()};
    auto const fighter_count{entities.num()};
    std::vector<EntityId> flat;
    std::uint32_t offset{};

    for (std::uint32_t capital_index{}; capital_index < capital_count; ++capital_index) {
        SCOPED_TRACE(::testing::PrintToString(capital_ids[capital_index]));
        std::vector<EntityId> expected;
        for (std::uint32_t fighter_index{}; fighter_index < fighter_count; ++fighter_index) {
            if (is_alive(healths.health(fighter_index)) &&
                parent_ids[fighter_index] == capital_ids[capital_index]) {
                expected.push_back(fighter_ids[fighter_index]);
            }
        }
        // Each capital selects a distinct ownership span.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        auto const actual{capitals.get_fighter_ids(capital_index)};
        EXPECT_EQ((std::vector<EntityId>{actual.begin(), actual.end()}), expected);
        EXPECT_EQ(spans[capital_index],
                  (IndexSpan{offset, static_cast<std::uint32_t>(expected.size())}));
        offset += static_cast<std::uint32_t>(expected.size());
        flat.insert(flat.end(), expected.begin(), expected.end());
    }
    auto const actual{capitals.get_fighter_ids()};
    EXPECT_EQ((std::vector<EntityId>{actual.begin(), actual.end()}), flat);
}
}
