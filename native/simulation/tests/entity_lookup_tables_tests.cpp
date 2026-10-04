#include "support/collision_agent_storage.h"
#include <ioj/sim/column_math.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/frame_vectors3f.h>
#include <ioj/sim/rotator_math.h>

#include <gtest/gtest.h>

#include <array>

namespace ioj::sim::tests {
TEST(EntityTypeRuns, GroupsWithoutChangingIdsAndOmitsEmptyIds) {
    std::array const ids{EntityUniqueId{3, EntityType::Fighter},
                         EntityUniqueId{},
                         EntityUniqueId{0, EntityType::PlayerShip},
                         EntityUniqueId{3, EntityType::Fighter}};
    auto const original{ids};
    std::array<std::uint32_t, ids.size()> order;
    auto const runs{group_entity_ids(ids, order)};
    EXPECT_EQ(ids, original);
    ASSERT_EQ(runs.num, 2u);
    EXPECT_EQ(runs.types[0], EntityType::PlayerShip);
    EXPECT_EQ(runs.types[1], EntityType::Fighter);
    EXPECT_EQ(runs.offsets[1], 1u);
    EXPECT_EQ(runs.counts[1], 2u);
    std::array<EntityUniqueId, ids.size()> sorted;
    for (std::size_t row{}; row < ids.size(); ++row) {
        sorted[row] = ids[order[row]];
    }
    auto const direct{entity_type_runs(sorted)};
    ASSERT_EQ(direct.num, runs.num);
    for (std::uint32_t run{}; run < runs.num; ++run) {
        EXPECT_EQ(direct.types[run], runs.types[run]);
        EXPECT_EQ(direct.offsets[run], runs.offsets[run]);
        EXPECT_EQ(direct.counts[run], runs.counts[run]);
    }
    EXPECT_EQ(group_entity_ids({}, {}).num, 0u);
#ifndef NDEBUG
    std::array const malformed{EntityUniqueId::from_raw(0x06000000)};
    EXPECT_DEATH(entity_type_runs(malformed), "EntityTypeRuns::capacity");
#endif
}

TEST(EntityLookupTables, PublishedCountIncludesDeadRowsUntilRepublished) {
    EntityLookupTable lookup{EntityType::Fighter};
    std::array const ids{EntityUniqueId{0, EntityType::Fighter},
                         EntityUniqueId{1, EntityType::Fighter}};
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
    std::array const fighters{EntityUniqueId{2, EntityType::Fighter}};
    std::array const capitals{EntityUniqueId{0, EntityType::CapitalShip}};
    lookups.for_type(EntityType::Fighter).publish_rows(fighters, std::array{Team::Blue});
    lookups.for_type(EntityType::CapitalShip).publish_rows(capitals, std::array{Team::Red});
    SimClockTestAccess::set_phase(clock, SimulationPhase::Thinking);
    std::array const ids{fighters[0],
                         EntityUniqueId{},
                         capitals[0],
                         fighters[0],
                         EntityUniqueId{999999, EntityType::Fighter}};
    std::array<std::uint32_t, ids.size()> order;
    std::array<EntityInstanceHandle, ids.size()> handles;
    lookups.lookup_handles(ids, order, handles);
    EXPECT_EQ(handles[0].team(), Team::Blue);
    EXPECT_FALSE(handles[1].is_valid());
    EXPECT_EQ(handles[2].team(), Team::Red);
    EXPECT_EQ(handles[0], handles[3]);
    EXPECT_FALSE(handles[4].is_valid());
}

TEST(EntityLookupTables, ResolvesTypeRunsAndPreservesDuplicateAndMissingSlots) {
    SimClock clock;
    EntityLookupTables lookups{clock};
    std::array const fighters{EntityUniqueId{2, EntityType::Fighter},
                              EntityUniqueId{5, EntityType::Fighter}};
    std::array const capitals{EntityUniqueId{0, EntityType::CapitalShip}};
    lookups.for_type(EntityType::Fighter).publish_rows(fighters, std::array{Team::Blue, Team::Red});
    lookups.for_type(EntityType::CapitalShip).publish_rows(capitals, std::array{Team::Green});
    SimClockTestAccess::set_phase(clock, SimulationPhase::Thinking);

    std::array const ids{capitals[0],
                         fighters[1],
                         fighters[0],
                         fighters[1],
                         EntityUniqueId{100, EntityType::Fighter},
                         EntityUniqueId{}};
    std::array<EntityInstanceHandle, ids.size()> handles;
    lookups.lookup_handles(ids, handles);
    EXPECT_EQ(handles[0].index(), 0u);
    EXPECT_EQ(handles[0].team(), Team::Green);
    EXPECT_EQ(handles[1].index(), 1u);
    EXPECT_EQ(handles[2].index(), 0u);
    EXPECT_EQ(handles[1], handles[3]);
    EXPECT_FALSE(handles[4].is_valid());
    EXPECT_FALSE(handles[5].is_valid());
    lookups.lookup_handles({}, {});
}

TEST(EntityLookupTables, RetirementAndReorderingCannotAliasStableIdentity) {
    EntityLookupTable lookup{EntityType::Fighter};
    std::array const ids{EntityUniqueId{0, EntityType::Fighter},
                         EntityUniqueId{1, EntityType::Fighter},
                         EntityUniqueId{2, EntityType::Fighter}};
    lookup.publish_rows(ids, {});
    lookup.retire(std::span{ids}.first(1));
    std::array const reordered{ids[2], ids[1]};
    lookup.publish_rows(reordered, {});
    std::array<EntityInstanceHandle, ids.size()> handles;
    lookup.lookup_handles(ids, handles);
    EXPECT_FALSE(handles[0].is_valid());
    EXPECT_EQ(handles[1].index(), 1u);
    EXPECT_EQ(handles[2].index(), 0u);
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
    std::array ids{fighter,
                   EntityUniqueId{},
                   capital,
                   fighter,
                   EntityUniqueId{99, EntityType::Fighter},
                   player,
                   dead};
    auto const original{ids};
    Vectors3f locations;
    Vectors3f velocities;
    locations.set_num(static_cast<std::uint32_t>(ids.size()));
    velocities.set_num(locations.num());
    SpatialQueryManager queries{owners.entity_tables};
    queries.initialise({{10, 10, 10}, {{100, 100, 100}}}, {});
    owners.refresh(queries);
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 4096> backing;
    ml::FrameMemoryResource scratch{backing};
    queries.copy_entity_locations(ids, locations.get_view(), &scratch);
    EXPECT_EQ(ids, original);
    std::array<EntityInstanceHandle, ids.size()> handles;
    queries.copy_entity_motion(ids, locations.get_view(), velocities.get_view(), handles, &scratch);
    EXPECT_EQ(ids, original);
    EXPECT_TRUE(handles[0].is_valid());
    EXPECT_FALSE(handles[1].is_valid());
    EXPECT_TRUE(handles[2].is_valid());
    EXPECT_TRUE(handles[3].is_valid());
    EXPECT_FALSE(handles[4].is_valid());
    EXPECT_FLOAT_EQ(locations[0].X, 10.f);
    EXPECT_FLOAT_EQ(locations[2].X, 40.f);
    EXPECT_FLOAT_EQ(locations[3].X, 10.f);
    EXPECT_FLOAT_EQ(locations[4].X, 0.f);
    EXPECT_FLOAT_EQ(velocities[2].X, 0.f);
    EXPECT_FLOAT_EQ(velocities[0].X, 1.f);
    EXPECT_FLOAT_EQ(locations[5].X, 70.f);
    EXPECT_FLOAT_EQ(velocities[5].X, 4.f);
    EXPECT_FALSE(handles[6].is_valid());
    EXPECT_FLOAT_EQ(locations[6].X, 0.f);
    EXPECT_FLOAT_EQ(velocities[6].X, 0.f);

    owners.remove(fighter);
    owners.publish();
    owners.refresh(queries);
    queries.copy_entity_motion(ids, locations.get_view(), velocities.get_view(), handles, &scratch);
    EXPECT_EQ(ids, original);
    EXPECT_FALSE(handles[0].is_valid());
    EXPECT_FALSE(handles[3].is_valid());
    EXPECT_FLOAT_EQ(locations[0].X, 0.f);
    EXPECT_FLOAT_EQ(velocities[0].X, 0.f);
}

TEST(EntityLookupTables, ResolutionHealthChangesBeforeHandlesRetire) {
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 4096> backing;
    ml::FrameMemoryResource scratch{backing};
    ml::FrameScratchScope scratch_scope{scratch};
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::Fighter, {{10, 0, 0}})};
    owners.publish();
    MissionManager mission{owners.clock, owners.ledger, owners.entity_tables};
    mission.set_mission_mode(MissionMode::SurviveTime);
    mission.add_entity_that_must_survive(id);
    mission.add_entity_required_to_kill(id);
    mission.begin_play(&scratch);
    ASSERT_EQ(mission.get_entity_health_that_must_survive()[0].health, 100);
    SimClockTestAccess::set_phase(owners.clock, SimulationPhase::Resolution);
    owners.set(id, {{20, 0, 0}}, {}, 0);
    mission.mission_tick(&scratch);
    EXPECT_EQ(mission.get_entity_health_that_must_survive()[0].health, 0);
    EXPECT_EQ(mission.get_entity_health_required_to_kill()[0].health, 0);
    EXPECT_EQ(mission.get_mission_state(), MissionState::Failed);
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

TEST(MissionObjectives, RejectHealthlessSurvivalAndRequiredKillObjectives) {
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::TubeSpinner)};
    MissionManager mission{owners.clock, owners.ledger, owners.entity_tables};
    EXPECT_DEATH(mission.add_entity_that_must_survive(id), "require an entity with health");
    EXPECT_DEATH(mission.add_entity_required_to_kill(id), "require an entity with health");
}
} // namespace ioj::sim::tests
