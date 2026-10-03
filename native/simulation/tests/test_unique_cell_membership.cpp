#include "support/collision_agent_storage.h"
#include <ioj/sim/spatial_query_manager.h>
#include <ioj/sim/testing/entity_observations.h>

#include <sandbox/core/frame_memory_resource.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

namespace ioj::sim::tests {
class UniqueCellMembership : public ::testing::Test {
  protected:
    UniqueCellMembership() {
        for (auto const type : ml::EnumTraits<EntityType>::values) {
            bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
        }
        queries.initialise({{8, 8, 8}, {{100.f, 100.f, 100.f}}}, bounds);
    }

    void rebuild() {
        owners.publish();
        queries.refresh_spatial_index();
    }

    CollisionAgentStorage owners;
    SpatialQueryManager queries{owners.entity_tables};
    collision::EntityAABBs bounds;
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 64 * 1024> backing{};
    ml::FrameMemoryResource memory{backing};
    ml::FrameScratchScope scope{memory};
    ml::FrameArray<EntityUniqueId> output{&scope.scratch()};
};

TEST_F(UniqueCellMembership, EmptyInputAndEmptyCellsReplacePreviousOutput) {
    auto const id{owners.spawn(EntityType::Fighter, {{50.f, 50.f, 50.f}})};
    rebuild();
    output.add(id);

    queries.collect_unique_entities_in_cells({}, output);
    EXPECT_TRUE(output.is_empty());

    output.add(id);
    std::array const cells{collision::CellCoord{0, 0, 0}};
    queries.collect_unique_entities_in_cells(cells, output);
    EXPECT_TRUE(output.is_empty());
}

TEST_F(UniqueCellMembership, OverlappingMembershipAndRepeatedCellsReturnEachEntityOnce) {
    auto const capital{owners.spawn(EntityType::CapitalShip, {{0.f, 50.f, 50.f}})};
    auto const fighter{owners.spawn(EntityType::Fighter, {{50.f, 50.f, 50.f}})};
    owners.spawn(EntityType::Turret, {{150.f, 50.f, 50.f}});
    rebuild();

    std::array const cells{collision::CellCoord{4, 4, 4},
                           collision::CellCoord{3, 4, 4},
                           collision::CellCoord{4, 4, 4}};
    queries.collect_unique_entities_in_cells(cells, output);

    ASSERT_EQ(output.num(), 2u);
    EXPECT_EQ(output[0], capital);
    EXPECT_EQ(output[1], fighter);
    EXPECT_TRUE(memory.owns(output.data()));
    EXPECT_EQ(memory.get_stats().current_root_claim_count, 1u);
}

TEST_F(UniqueCellMembership, OrderIsIndependentOfCellOrderAndOutputIsReused) {
    auto const first{owners.spawn(EntityType::Fighter, {{-50.f, 50.f, 50.f}})};
    auto const second{owners.spawn(EntityType::Fighter, {{50.f, 50.f, 50.f}})};
    rebuild();

    std::array cells{collision::CellCoord{4, 4, 4}, collision::CellCoord{3, 4, 4}};
    queries.collect_unique_entities_in_cells(cells, output);
    ASSERT_EQ(output.num(), 2u);
    EXPECT_EQ(output[0], first);
    EXPECT_EQ(output[1], second);
    auto const claims{memory.get_stats().current_root_claim_count};

    std::ranges::reverse(cells);
    queries.collect_unique_entities_in_cells(cells, output);
    ASSERT_EQ(output.num(), 2u);
    EXPECT_EQ(output[0], first);
    EXPECT_EQ(output[1], second);
    EXPECT_EQ(memory.get_stats().current_root_claim_count, claims);

    queries.collect_unique_entities_in_cells(std::span{cells}.first(1), output);
    ASSERT_EQ(output.num(), 1u);
    EXPECT_EQ(output[0], first);
}

TEST_F(UniqueCellMembership, IncludesAllTeamsAndMoreThan128Entities) {
    constexpr std::uint32_t count{200};
    for (std::uint32_t index{}; index < count; ++index) {
        owners.spawn(EntityType::Fighter,
                     {{50.f, 50.f, 50.f}},
                     {},
                     100,
                     index % 2 == 0 ? Team::Blue : Team::Red);
    }
    rebuild();

    std::array const cells{collision::CellCoord{4, 4, 4}};
    queries.collect_unique_entities_in_cells(cells, output);
    EXPECT_EQ(output.num(), count);
    EXPECT_TRUE(std::ranges::equal(output.view(), owners.fighters.get_const_view().entity_ids()));
}

TEST_F(UniqueCellMembership, MembershipChangesOnlyWhenGridIsRebuilt) {
    auto const id{owners.spawn(EntityType::Fighter, {{50.f, 50.f, 50.f}})};
    rebuild();
    owners.set(id, {{150.f, 50.f, 50.f}}, {}, 0);

    std::array const cells{collision::CellCoord{4, 4, 4}};
    queries.collect_unique_entities_in_cells(cells, output);
    ASSERT_EQ(output.num(), 1u);
    EXPECT_EQ(output[0], id);

    rebuild();
    queries.collect_unique_entities_in_cells(cells, output);
    EXPECT_TRUE(output.is_empty());
}

TEST_F(UniqueCellMembership, StaticGeometryIsNotEntityMembership) {
    rebuild();
    queries.add_static_collision_aabb({{40.f, 40.f, 40.f}}, {{60.f, 60.f, 60.f}});

    std::array const cells{collision::CellCoord{4, 4, 4}};
    queries.collect_unique_entities_in_cells(cells, output);
    EXPECT_TRUE(output.is_empty());
}
} // namespace ioj::sim::tests
