#include <ioj/sim/entity_tables.h>
#include <ioj/sim/testing/sim_clock_test_access.h>

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace ioj::sim::tests {
TEST(HealthTable, SeparatesFixedTypeRangesAndCopiesBulkValues) {
    HealthTable health;
    std::array const fighters{EntityUniqueId{0, EntityType::Fighter},
                              EntityUniqueId{1, EntityType::Fighter}};
    std::array const capitals{EntityUniqueId{0, EntityType::CapitalShip}};
    health.initialise_rows<EntityType::Fighter>(0, std::array<Health, 2>{10, 20});
    health.initialise_rows<EntityType::CapitalShip>(0, capitals.size(), 900);
    auto const fighter_health{health.get_view<EntityType::Fighter>(fighters.size())};
    fighter_health.copy_from(std::array<Health, 2>{30, 40});
    std::array<Health, 2> copied;
    health.get_const_view<EntityType::Fighter>(fighters.size()).copy_to(copied);
    EXPECT_EQ(copied, (std::array<Health, 2>{30, 40}));
    EXPECT_EQ(health.get_const_view<EntityType::CapitalShip>(capitals.size()).health(0), 900);
}

TEST(HealthTable, MirrorsOwnerSwapRemovalForBoundaryAndBatchShapes) {
    auto run_case = [](std::span<std::uint32_t const> removals) {
        HealthTable health;
        std::vector<EntityUniqueId> owners;
        std::array<Health, 6> values{10, 20, 30, 40, 50, 60};
        auto const value_count{values.size()};
        for (std::uint32_t row{}; row < value_count; ++row) {
            owners.emplace_back(row, EntityType::Fighter);
        }
        health.initialise_rows<EntityType::Fighter>(0, values);
        health.remove_rows<EntityType::Fighter>(owners.size(), removals);
        for (auto row : removals) {
            owners[row] = owners.back();
            owners.pop_back();
        }
        auto const view{health.get_const_view<EntityType::Fighter>(owners.size())};
        ASSERT_EQ(view.num(), owners.size());
        auto const count{view.num()};
        for (std::uint32_t row{}; row < count; ++row) {
            EXPECT_EQ(view.health(row), values[owners[row].index()]);
        }
    };
    run_case({});
    run_case(std::array{0u});
    run_case(std::array{2u});
    run_case(std::array{5u});
    run_case(std::array{3u, 2u});
    run_case(std::array{5u, 3u, 1u});
    run_case(std::array{5u, 4u, 3u, 2u, 1u, 0u});
}

TEST(HealthTable, WritesHealthDirectlyThroughBorrowedViews) {
    HealthTable health;
    health.initialise_rows<EntityType::Fighter>(0, 2, 100);
    auto const view{health.get_view<EntityType::Fighter>(2)};
    auto const read{health.get_const_view<EntityType::Fighter>(2)};

    view.set_health(0, 100);
    view.set_health(1, 80);
    EXPECT_EQ(read.health(0), 100);
    EXPECT_EQ(read.health(1), 80);

    view.set_health(1, 60);
    EXPECT_EQ(read.health(1), 60);
}

TEST(EntityTables, RebuildsHandlesAfterOwnerReorderingAndMaximumHealthChanges) {
    SimClock clock;
    EntityTables tables{clock};
    std::array const ids{EntityUniqueId{2, EntityType::Fighter},
                         EntityUniqueId{6, EntityType::Fighter}};
    std::array const teams{Team::Blue, Team::Red};
    tables.health.initialise_rows<EntityType::Fighter>(0, std::array<Health, 2>{50, 100});
    tables.publish<EntityType::Fighter>(ids, teams, 100);
    auto& lookup{tables.lookups.for_type(EntityType::Fighter)};
    std::array<EntityInstanceHandle, 2> handles;
    lookup.lookup_handles(ids, handles);
    EXPECT_EQ(handles[0].health_state(), 1u);
    EXPECT_EQ(handles[1].health_state(), 3u);

    tables.health.get_view<EntityType::Fighter>(ids.size()).set_health(1, 25);
    lookup.lookup_handles(ids, handles);
    EXPECT_EQ(handles[1].health_state(), 3u);

    std::array<std::int32_t, 2> order{1, 0};
    tables.health.apply_permutation<EntityType::Fighter>(order);
    tables.publish<EntityType::Fighter>(
        std::array{ids[1], ids[0]}, std::array{teams[1], teams[0]}, 100);
    lookup.lookup_handles(ids, handles);
    EXPECT_EQ(handles[0].index(), 1u);
    EXPECT_EQ(handles[0].health_state(), 1u);
    EXPECT_EQ(handles[1].index(), 0u);
    EXPECT_EQ(handles[1].health_state(), 0u);
    EXPECT_EQ(handles[1].team(), Team::Red);

    // Rebuild metadata even when the stored health values have not changed.
    tables.publish<EntityType::Fighter>(
        std::array{ids[1], ids[0]}, std::array{teams[1], teams[0]}, 50);
    lookup.lookup_handles(ids, handles);
    EXPECT_EQ(handles[0].health_state(), 3u);
    EXPECT_EQ(handles[1].health_state(), 1u);
}

TEST(EntityTables, KeepsRetiredIdsInvalidAndOtherTypeHealthUnchanged) {
    SimClock clock;
    EntityTables tables{clock};
    std::array const ids{EntityUniqueId{0, EntityType::Fighter},
                         EntityUniqueId{1, EntityType::Fighter}};
    std::array const player{EntityUniqueId{0, EntityType::PlayerShip}};
    tables.health.initialise_rows<EntityType::Fighter>(0, ids.size(), 100);
    tables.health.initialise_rows<EntityType::PlayerShip>(0, player.size(), 75);
    tables.publish<EntityType::Fighter>(ids, {}, 100);
    tables.health.get_view<EntityType::Fighter>(ids.size()).set_health(0, 0);
    tables.publish<EntityType::Fighter>(ids, {}, 100);
    EXPECT_FALSE(tables.lookups.for_type(EntityType::Fighter).entries()[0].is_valid());
    tables.lookups.for_type(EntityType::Fighter).retire(std::span{ids}.first(1));
    tables.health.remove_rows<EntityType::Fighter>(ids.size(), std::array{0u});
    tables.publish<EntityType::Fighter>(std::span{ids}.last(1), {}, 100);
    EXPECT_EQ(tables.health.get_const_view<EntityType::PlayerShip>(player.size()).health(0), 75);
    EXPECT_FALSE(tables.lookups.for_type(EntityType::Fighter).entries()[0].is_valid());
    EXPECT_EQ(tables.lookups.for_type(EntityType::Fighter).entries()[1].index(), 0u);
}

TEST(EntityTables, QuantisesQuarterBoundariesAndClampsOverheal) {
    EXPECT_EQ(quantise_health(1, 100), 0u);
    EXPECT_EQ(quantise_health(25, 100), 0u);
    EXPECT_EQ(quantise_health(26, 100), 1u);
    EXPECT_EQ(quantise_health(50, 100), 1u);
    EXPECT_EQ(quantise_health(51, 100), 2u);
    EXPECT_EQ(quantise_health(75, 100), 2u);
    EXPECT_EQ(quantise_health(76, 100), 3u);
    EXPECT_EQ(quantise_health(150, 100), 3u);
}
} // namespace ioj::sim::tests
