#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/tasks/executor_table.h>

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace ioj::sim::tests {
TEST(HealthTable, SeparatesFixedTypeRangesAndCopiesBulkValues) {
    HealthTable health;
    std::array const fighters{
        EntityId{entity_lifetime_capacities[EntityType::Fighter] - 1, EntityType::Fighter},
        EntityId{2, EntityType::Fighter}};
    std::array const capitals{EntityId{3, EntityType::CapitalShip}};
    health.initialise_entities<EntityType::Fighter>(fighters,
                                                    std::array<Health, 2>{Health{10}, Health{20}});
    health.initialise_entities<EntityType::CapitalShip>(capitals, Health{900});
    auto const fighter_health{health.view<EntityType::Fighter>()};
    fighter_health.copy_from(fighters, std::array<Health, 2>{Health{30}, Health{40}});
    std::array<Health, 2> copied;
    health.const_view<EntityType::Fighter>().copy_to(fighters, copied);
    EXPECT_EQ(copied, (std::array<Health, 2>{Health{30}, Health{40}}));
    EXPECT_EQ(health.const_view<EntityType::CapitalShip>().health(capitals[0]).value(), 900);
}

TEST(EntityTables, PreservesLifetimeSlotsWhenOwnerRowsMove) {
    auto run_case = [](std::span<std::uint32_t const> removals) {
        HealthTable health;
        tasks::ExecutorTable executors;
        std::vector<EntityId> owners;
        std::array<Health, 6> values{
            Health{10}, Health{20}, Health{30}, Health{40}, Health{50}, Health{60}};
        auto const value_count{values.size()};
        for (std::uint32_t row{}; row < value_count; ++row) {
            owners.emplace_back(row, EntityType::Fighter);
        }
        health.initialise_entities<EntityType::Fighter>(owners, values);
        executors.initialise_entities<EntityType::Fighter>(owners);
        auto const executor_view{executors.view<EntityType::Fighter>()};
        for (auto const id : owners) {
            executor_view.current_tasks[id.index()] = tasks::TaskId{id.index()};
            executor_view.current_instructions[id.index()] =
                tasks::InstructionHandle{id.index() + 10};
        }
        for (auto row : removals) {
            owners[row] = owners.back();
            owners.pop_back();
        }
        auto const view{health.const_view<EntityType::Fighter>()};
        auto const count{owners.size()};
        for (std::uint32_t row{}; row < count; ++row) {
            EXPECT_EQ(view.health(owners[row]), values[owners[row].index()]);
            EXPECT_EQ(executor_view.current_tasks[owners[row].index()],
                      tasks::TaskId{owners[row].index()});
            EXPECT_FALSE(executor_view.pending_tasks[owners[row].index()].is_valid());
            EXPECT_EQ(executor_view.current_instructions[owners[row].index()],
                      tasks::InstructionHandle{owners[row].index() + 10});
        }
        for (std::uint32_t index{}; index < value_count; ++index) {
            EXPECT_EQ(view.health(EntityId{index, EntityType::Fighter}), values[index]);
            EXPECT_EQ(executor_view.current_tasks[index], tasks::TaskId{index});
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
    std::array const ids{EntityId{7, EntityType::Fighter}, EntityId{19, EntityType::Fighter}};
    health.initialise_entities<EntityType::Fighter>(ids, Health{100});
    auto const view{health.view<EntityType::Fighter>()};
    auto const read{health.const_view<EntityType::Fighter>()};

    view.set_health(ids[0], Health{100});
    view.set_health(ids[1], Health{80});
    EXPECT_EQ(read.health(ids[0]).value(), 100);
    EXPECT_EQ(read.health(ids[1]).value(), 80);

    view.set_health(ids[1], Health{60});
    EXPECT_EQ(read.health(ids[1]).value(), 60);
}

TEST(EntityTables, RebuildsHandlesAfterOwnerReorderingAndMaximumHealthChanges) {
    SimClock clock;
    EntityTables tables{clock};
    std::array const ids{EntityId{2, EntityType::Fighter}, EntityId{6, EntityType::Fighter}};
    std::array const teams{Team::Blue, Team::Red};
    tables.health.initialise_entities<EntityType::Fighter>(
        ids, std::array<Health, 2>{Health{50}, Health{100}});
    tables.publish<EntityType::Fighter>(ids, teams, Health{100});
    auto& lookup{tables.lookups.for_type(EntityType::Fighter)};
    std::array<EntityInstanceHandle, 2> handles;
    lookup.lookup_handles(ids, handles);
    EXPECT_EQ(handles[0].health_state(), 1u);
    EXPECT_EQ(handles[1].health_state(), 3u);

    tables.health.view<EntityType::Fighter>().set_health(ids[1], Health{25});
    lookup.lookup_handles(ids, handles);
    EXPECT_EQ(handles[1].health_state(), 3u);

    tables.publish<EntityType::Fighter>(
        std::array{ids[1], ids[0]}, std::array{teams[1], teams[0]}, Health{100});
    lookup.lookup_handles(ids, handles);
    EXPECT_EQ(handles[0].index(), 1u);
    EXPECT_EQ(handles[0].health_state(), 1u);
    EXPECT_EQ(handles[1].index(), 0u);
    EXPECT_EQ(handles[1].health_state(), 0u);
    EXPECT_EQ(handles[1].team(), Team::Red);

    // Rebuild metadata even when the stored health values have not changed.
    tables.publish<EntityType::Fighter>(
        std::array{ids[1], ids[0]}, std::array{teams[1], teams[0]}, Health{50});
    lookup.lookup_handles(ids, handles);
    EXPECT_EQ(handles[0].health_state(), 3u);
    EXPECT_EQ(handles[1].health_state(), 1u);
}

TEST(EntityTables, KeepsRetiredIdsInvalidAndOtherTypeHealthUnchanged) {
    SimClock clock;
    EntityTables tables{clock};
    std::array const ids{EntityId{7, EntityType::Fighter}, EntityId{2, EntityType::Fighter}};
    std::array const player{EntityId{0, EntityType::PlayerShip}};
    tables.health.initialise_entities<EntityType::Fighter>(ids, Health{100});
    tables.health.initialise_entities<EntityType::PlayerShip>(player, Health{75});
    tables.publish<EntityType::Fighter>(ids, {}, Health{100});
    tables.health.view<EntityType::Fighter>().set_health(ids[0], Health{0});
    tables.publish<EntityType::Fighter>(ids, {}, Health{100});
    EXPECT_FALSE(tables.lookups.for_type(EntityType::Fighter).entries()[ids[0].index()].is_valid());
    tables.lookups.for_type(EntityType::Fighter).retire(std::span{ids}.first(1));
    tables.publish<EntityType::Fighter>(std::span{ids}.last(1), {}, Health{100});
    EXPECT_EQ(tables.health.const_view<EntityType::PlayerShip>().health(player[0]).value(), 75);
    EXPECT_FALSE(tables.lookups.for_type(EntityType::Fighter).entries()[ids[0].index()].is_valid());
    EXPECT_EQ(tables.lookups.for_type(EntityType::Fighter).entries()[ids[1].index()].index(), 0u);
    EXPECT_EQ(tables.health.health(ids[0]), Health{0});
}

TEST(EntityTables, QuantisesQuarterBoundariesAndClampsOverheal) {
    EXPECT_EQ(quantise_health(Health{1}, Health{100}), 0u);
    EXPECT_EQ(quantise_health(Health{25}, Health{100}), 0u);
    EXPECT_EQ(quantise_health(Health{26}, Health{100}), 1u);
    EXPECT_EQ(quantise_health(Health{50}, Health{100}), 1u);
    EXPECT_EQ(quantise_health(Health{51}, Health{100}), 2u);
    EXPECT_EQ(quantise_health(Health{75}, Health{100}), 2u);
    EXPECT_EQ(quantise_health(Health{76}, Health{100}), 3u);
    EXPECT_EQ(quantise_health(Health{150}, Health{100}), 3u);
}
} // namespace ioj::sim::tests
