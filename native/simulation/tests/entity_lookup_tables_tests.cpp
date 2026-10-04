#include "support/collision_agent_storage.h"
#include <ioj/sim/entity_queries.h>

#include <gtest/gtest.h>

#include <array>

namespace ioj::sim::tests {
TEST(EntityTypeRuns, GroupsWithoutChangingIdsAndOmitsInvalidTypes) {
    std::array const ids{EntityUniqueId{3, EntityType::Fighter},
                         EntityUniqueId{},
                         EntityUniqueId{0, EntityType::PlayerShip},
                         EntityUniqueId{3, EntityType::Fighter},
                         EntityUniqueId::from_raw(0x06000000)};
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
    lookups.resolve(ids, order, handles);
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
    lookups.resolve(ids, handles);
    EXPECT_EQ(handles[0].index(), 0u);
    EXPECT_EQ(handles[0].team(), Team::Green);
    EXPECT_EQ(handles[1].index(), 1u);
    EXPECT_EQ(handles[2].index(), 0u);
    EXPECT_EQ(handles[1], handles[3]);
    EXPECT_FALSE(handles[4].is_valid());
    EXPECT_FALSE(handles[5].is_valid());
    lookups.resolve({}, {});
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
    lookup.resolve(ids, handles);
    EXPECT_FALSE(handles[0].is_valid());
    EXPECT_EQ(handles[1].index(), 1u);
    EXPECT_EQ(handles[2].index(), 0u);
}

TEST(EntityQueries, GathersMixedTypesInCallerOrderWithoutChangingInput) {
    CollisionAgentStorage owners;
    auto const fighter{owners.spawn(EntityType::Fighter, {{10, 20, 30}}, {}, 80, Team::Blue)};
    auto const capital{owners.spawn(EntityType::CapitalShip, {{40, 50, 60}}, {}, 95, Team::Red)};
    owners.publish();
    std::array const ids{
        fighter, EntityUniqueId{}, capital, fighter, EntityUniqueId{99, EntityType::Fighter}};
    auto const original{ids};
    std::array<std::uint32_t, ids.size()> order;
    std::array<Health, ids.size()> healths;
    std::array<std::uint8_t, ids.size()> alive;
    std::array<Team, ids.size()> teams;
    Vectors3f locations;
    locations.set_num(static_cast<std::uint32_t>(ids.size()));
    gather_entities(
        owners.entity_tables,
        ids,
        order,
        {.locations = locations.get_view(), .teams = teams, .alive = alive, .healths = healths});
    EXPECT_EQ(ids, original);
    EXPECT_EQ(healths, (std::array<Health, 5>{80, 0, 95, 80, 0}));
    EXPECT_EQ(alive, (std::array<std::uint8_t, 5>{1, 0, 1, 1, 0}));
    EXPECT_FLOAT_EQ(locations[0].X, 10.f);
    EXPECT_FLOAT_EQ(locations[2].X, 40.f);
    EXPECT_FLOAT_EQ(locations[3].X, 10.f);
    EXPECT_EQ(teams[2], Team::Red);
}

TEST(EntityQueries, ReadsResolutionHealthBeforeCompactionAndRetiresAtCommit) {
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::Fighter, {{10, 0, 0}})};
    owners.publish();
    SimClockTestAccess::set_phase(owners.clock, SimulationPhase::Resolution);
    owners.set(id, {{20, 0, 0}}, {}, 0);
    std::array const ids{id};
    std::array<std::uint32_t, 1> order;
    std::array<Health, 1> healths;
    std::array<std::uint8_t, 1> alive;
    gather_entities(owners.entity_tables, ids, order, {.alive = alive, .healths = healths});
    EXPECT_EQ(healths[0], 0);
    EXPECT_FALSE(alive[0]);
    EXPECT_TRUE(owners.entity_tables.lookups.for_type(EntityType::Fighter)
                    .entries()[id.index()]
                    .is_valid());
    owners.remove(id);
    owners.publish();
    std::array<EntityInstanceHandle, 1> handles;
    owners.entity_tables.lookups.resolve(ids, handles);
    EXPECT_FALSE(handles[0].is_valid());
    EXPECT_TRUE(owners.ledger.is_valid_unique_id(id));
}
} // namespace ioj::sim::tests
