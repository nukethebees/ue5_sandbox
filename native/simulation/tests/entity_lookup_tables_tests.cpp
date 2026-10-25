#include "support/collision_agent_storage.h"
#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/testing/math_diff.h>
#include <ioj/testing/structural_diff.h>
#include <ioj/vectors3f.h>

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace ioj::sim {
void diff_members(EntityInstanceHandle const expected,
                  EntityInstanceHandle const actual,
                  DiffContext& context) {
    context.diff("valid", expected.is_valid(), actual.is_valid());
    if (expected.is_valid() && actual.is_valid()) {
        context.diff("index", expected.index(), actual.index());
        context.diff("team", expected.team(), actual.team());
        context.diff("health_state", expected.health_state(), actual.health_state());
    }
}
}

namespace ioj::sim::tests {
using ioj::testing::structurally_equal;
using Handle = EntityInstanceHandle;

auto vector_values(Vectors3f const& vectors) -> std::vector<Vector3f> {
    auto const values{vectors.get_const_view().resolve()};
    auto const count{values.num()};
    std::vector<Vector3f> result(count);
    for (std::uint32_t row{}; row < count; ++row) {
        result[row] = values[row];
    }
    return result;
}
TEST(EntityTypeRuns, GroupsWithoutChangingIdsAndOmitsEmptyIds) {
    std::array const ids{EntityId{3, EntityType::Fighter},
                         EntityId{},
                         EntityId{0, EntityType::PlayerShip},
                         EntityId{3, EntityType::Fighter}};
    auto const original{ids};
    std::array<std::uint32_t, ids.size()> order;
    auto const runs{group_entity_ids(ids, order)};
    EXPECT_EQ(ids, original);
    ASSERT_EQ(runs.num, 2u);
    EXPECT_EQ(runs.types[0], EntityType::PlayerShip);
    EXPECT_EQ(runs.types[1], EntityType::Fighter);
    EXPECT_EQ(runs.offsets[1], 1u);
    EXPECT_EQ(runs.counts[1], 2u);
    std::array<EntityId, ids.size()> sorted;
    auto const count{ids.size()};
    for (std::size_t row{}; row < count; ++row) {
        sorted[row] = ids[order[row]];
    }
    auto const direct{entity_type_runs(sorted)};
    ASSERT_EQ(direct.num, runs.num);
    EXPECT_PRED_FORMAT2(structurally_equal, runs.types, direct.types);
    EXPECT_PRED_FORMAT2(structurally_equal, runs.offsets, direct.offsets);
    EXPECT_PRED_FORMAT2(structurally_equal, runs.counts, direct.counts);
    EXPECT_EQ(group_entity_ids({}, {}).num, 0u);
#ifndef NDEBUG
    std::array const malformed{EntityId::from_raw(0x06000000)};
    EXPECT_DEATH(entity_type_runs(malformed), "EntityTypeRuns::capacity");
#endif
}

TEST(EntityLookupTables, PublishedCountIncludesDeadRowsUntilRepublished) {
    EntityLookupTable lookup{EntityType::Fighter};
    std::array const ids{EntityId{0, EntityType::Fighter}, EntityId{1, EntityType::Fighter}};
    lookup.publish_rows(ids, {}, std::array<Health, 2>{0, 100}, 100);
    EXPECT_EQ(lookup.row_count(), 2u);
    EXPECT_FALSE(lookup.entries()[0].is_valid());
    lookup.retire(std::span{ids}.last(1));
    EXPECT_EQ(lookup.row_count(), 2u);
    lookup.publish_rows(std::span{ids}.last(1), {});
    EXPECT_EQ(lookup.row_count(), 1u);
    EXPECT_EQ(lookup.entries()[1].index(), 0u);
    lookup.publish_rows({}, {});
    EXPECT_EQ(lookup.row_count(), 0u);
}

TEST(EntityLookupTables, ResolvesArbitraryIdsInCallerOrder) {
    SimClock clock;
    EntityLookupTables lookups{clock};
    std::array const fighters{EntityId{2, EntityType::Fighter}, EntityId{3, EntityType::Fighter}};
    std::array const capitals{EntityId{0, EntityType::CapitalShip}};
    lookups.for_type(EntityType::Fighter)
        .publish_rows(fighters, std::array{Team::Blue, Team::Blue});
    lookups.for_type(EntityType::CapitalShip).publish_rows(capitals, std::array{Team::Red});
    lookups.for_type(EntityType::Fighter).retire(std::span{fighters}.last(1));
    SimClockTestAccess::set_phase(clock, SimulationPhase::Thinking);
    std::array const ids{fighters[0], EntityId{}, capitals[0], fighters[0], fighters[1]};
    std::array<std::uint32_t, ids.size()> order;
    std::array<EntityInstanceHandle, ids.size()> handles;
    lookups.lookup_handles(ids, order, handles);
    std::array<Handle, 5> const expected{
        {Handle{0, Team::Blue, 3}, {}, Handle{0, Team::Red, 3}, Handle{0, Team::Blue, 3}, {}}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, handles);
}

TEST(EntityLookupTables, ResolvesTypeRunsAndPreservesDuplicateAndMissingSlots) {
    SimClock clock;
    EntityLookupTables lookups{clock};
    std::array const fighters{EntityId{2, EntityType::Fighter}, EntityId{5, EntityType::Fighter}};
    std::array const capitals{EntityId{0, EntityType::CapitalShip}};
    lookups.for_type(EntityType::Fighter).publish_rows(fighters, std::array{Team::Blue, Team::Red});
    lookups.for_type(EntityType::CapitalShip).publish_rows(capitals, std::array{Team::Green});
    SimClockTestAccess::set_phase(clock, SimulationPhase::Thinking);

    std::array const ids{capitals[0],
                         fighters[1],
                         fighters[0],
                         fighters[1],
                         EntityId{100, EntityType::Fighter},
                         EntityId{}};
    std::array<EntityInstanceHandle, ids.size()> handles;
    lookups.lookup_handles(ids, handles);
    std::array<Handle, 6> const expected{{Handle{0, Team::Green, 3},
                                          Handle{1, Team::Red, 3},
                                          Handle{0, Team::Blue, 3},
                                          Handle{1, Team::Red, 3},
                                          {},
                                          {}}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, handles);
    lookups.lookup_handles({}, {});
}

TEST(EntityLookupTables, RetirementAndReorderingCannotAliasStableIdentity) {
    EntityLookupTable lookup{EntityType::Fighter};
    std::array const ids{EntityId{0, EntityType::Fighter},
                         EntityId{1, EntityType::Fighter},
                         EntityId{2, EntityType::Fighter}};
    lookup.publish_rows(ids, {});
    lookup.retire(std::span{ids}.first(1));
    std::array const reordered{ids[2], ids[1]};
    lookup.publish_rows(reordered, {});
    std::array<EntityInstanceHandle, ids.size()> handles;
    lookup.lookup_handles(ids, handles);
    std::array<Handle, 3> const expected{
        {{}, Handle{1, Team::White, 3}, Handle{0, Team::White, 3}}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, handles);
}

TEST(EntityMotion, CopiesMixedTypesAndReportsMissingEntitiesWithoutChangingIds) {
    CollisionAgentStorage owners;
    auto const fighter{owners.spawn(EntityType::Fighter, {{10, 20, 30}}, {}, 80, Team::Blue)};
    auto const capital{owners.spawn(EntityType::CapitalShip, {{40, 50, 60}}, {}, 95, Team::Red)};
    auto const player{owners.spawn(EntityType::PlayerShip, {{70, 80, 90}})};
    auto const dead{owners.spawn(EntityType::Fighter, {}, {}, 0)};
    owners.fighters.get_view().view_velocities().set(0, {{1, 2, 3}});
    owners.player_velocity = {4, 5, 6};
    owners.publish();
    std::array ids{
        fighter, EntityId{}, capital, fighter, EntityId{99, EntityType::Fighter}, player, dead};
    auto const original{ids};
    Vectors3f locations;
    Vectors3f velocities;
    locations.set_num(static_cast<std::uint32_t>(ids.size()));
    velocities.set_num(locations.num());
    SpatialQueryManager queries{owners.entity_tables};
    queries.initialise({{10, 10, 10}, {{100, 100, 100}}}, {});
    owners.refresh(queries);
    alignas(FrameMemoryResource::backing_alignment) std::array<std::byte, 4096> backing;
    FrameMemoryResource scratch{backing};
    queries.copy_entity_locations(ids, locations.get_view().resolve(), &scratch);
    EXPECT_EQ(ids, original);
    std::array<EntityInstanceHandle, ids.size()> handles;
    queries.copy_entity_motion(
        ids, locations.get_view().resolve(), velocities.get_view().resolve(), handles, &scratch);
    std::array expected_validity{true, false, true, true, false, true, false};
    std::vector<Vector3f> expected_locations{
        {10, 20, 30}, {}, {40, 50, 60}, {10, 20, 30}, {}, {70, 80, 90}, {}};
    std::vector<Vector3f> expected_velocities{{1, 2, 3}, {}, {}, {1, 2, 3}, {}, {4, 5, 6}, {}};
    auto const expect_motion{[&] {
        std::array<bool, ids.size()> validity;
        std::ranges::transform(handles, validity.begin(), &EntityInstanceHandle::is_valid);
        EXPECT_PRED_FORMAT2(structurally_equal, original, ids);
        EXPECT_PRED_FORMAT2(structurally_equal, expected_validity, validity);
        EXPECT_PRED_FORMAT2(structurally_equal, expected_locations, vector_values(locations));
        EXPECT_PRED_FORMAT2(structurally_equal, expected_velocities, vector_values(velocities));
    }};
    expect_motion();

    owners.remove(fighter);
    owners.publish();
    owners.refresh(queries);
    queries.copy_entity_motion(
        ids, locations.get_view().resolve(), velocities.get_view().resolve(), handles, &scratch);
    expected_validity[0] = false;
    expected_validity[3] = false;
    expected_locations[0] = {};
    expected_locations[3] = {};
    expected_velocities[0] = {};
    expected_velocities[3] = {};
    expect_motion();
}

TEST(EntityLookupTables, ResolutionHealthChangesBeforeHandlesRetire) {
    alignas(FrameMemoryResource::backing_alignment) std::array<std::byte, 4096> backing;
    FrameMemoryResource scratch{backing};
    FrameScratchScope scratch_scope{scratch};
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::Fighter, {{10, 0, 0}})};
    owners.publish();
    SimClockTestAccess::set_phase(owners.clock, SimulationPhase::Resolution);
    owners.set(id, {{20, 0, 0}}, {}, 0);
    std::array const ids{id};
    EXPECT_EQ(owners.health_table.get_const_view<EntityType::Fighter>(1).health(0), 0);
    EXPECT_TRUE(owners.entity_tables.lookups.for_type(EntityType::Fighter)
                    .entries()[id.index()]
                    .is_valid());
    owners.remove(id);
    owners.publish();
    std::array<EntityInstanceHandle, 1> handles;
    owners.entity_tables.lookups.lookup_handles(ids, handles);
    EXPECT_FALSE(handles[0].is_valid());
    EXPECT_TRUE(owners.ledger.is_valid_unique_id(id));
}

} // namespace ioj::sim::tests
