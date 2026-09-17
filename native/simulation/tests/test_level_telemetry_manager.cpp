#include <bit>
#include <cstdint>
#include <utility>

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/level_telemetry_manager.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/spatial_query_manager.h>

#include <gtest/gtest.h>

namespace ioj::sim {

struct LevelTelemetryManagerTestAccess {
    static auto history(LevelTelemetryManager const& manager) -> LevelTelemetryBlockHistory const& {
        return manager.history_;
    }
};

} // namespace ioj::sim

namespace ioj::sim::tests {

TEST(LevelTelemetryManager, RecordsAndReusesHistory) {
    using Field = telemetry::HistoryField;
    using FieldMask = telemetry::HistoryFieldMask;
    static_assert(FieldMask::index(Field::ActiveEntities) == 0);
    static_assert(FieldMask::index(Field::ActiveEntitiesByType) == 1);
    static_assert(FieldMask::index(Field::ActiveEntitiesByTeamAndType) == 6);
    static_assert(FieldMask::index(Field::SpawnedEntities) == 36);
    static_assert(FieldMask::index(Field::LasersFired) == 40);
    static_assert(FieldMask::field_count == 41);
    static_assert(sizeof(FieldMask) == sizeof(std::uint64_t));

    SimClock clock;
    clock.initialise({});
    EntityLedger entity_ledger;
    CombatEvents combat_events{entity_ledger};
    AgentIndexes indexes{clock};
    HealthTable health_table;
    AgentAccessor agents{indexes, health_table};
    SpatialQueryManager spatial_queries{agents};
    lasers::Sim lasers{clock, combat_events, spatial_queries};
    GameMemory game_memory{{.root_capacity_bytes = 2u * 1024u * 1024u}};
    LevelTelemetryManager telemetry_manager{
        clock,
        entity_ledger,
        lasers,
        game_memory,
        {.block_bytes = 100u * 1024u},
    };

    telemetry_manager.initialise();
    auto const& active_count_data{telemetry_manager.get_active_entity_count_data()};
    auto const& kill_count_data{telemetry_manager.get_cumulative_kill_count_data()};
    auto tick_series{telemetry_manager.materialize_tick_series()};
    auto const& initial_state{telemetry_manager.get_current_state()};
    EXPECT_EQ(active_count_data.num(), 1);
    EXPECT_EQ(active_count_data.last_time(), 0);
    EXPECT_EQ(active_count_data.last_value(), 0);
    EXPECT_EQ(kill_count_data.num(), 1);
    EXPECT_EQ(kill_count_data.last_value(), 0);

    EXPECT_EQ(initial_state.active_lasers, 0);
    EXPECT_EQ(tick_series.active_lasers.num(), 1);
    auto const& initial_history{LevelTelemetryManagerTestAccess::history(telemetry_manager)};
    auto const initial_history_columns{initial_history.block_view(0).columns()};
    EXPECT_EQ(initial_history.num(), 1);
    EXPECT_EQ(initial_history_columns.validity_masks[0].value(),
              (std::uint64_t{1} << FieldMask::field_count) - 1);
    EXPECT_EQ(initial_history_columns.kills[0], 0);
    EXPECT_EQ(tick_series.kills.last_value(), 0);
    EXPECT_LE(sizeof(LevelTelemetryManager), 1216);
    bool columns_aligned{true};
    initial_history_columns.each_column([&columns_aligned](auto const column) {
        columns_aligned =
            columns_aligned && reinterpret_cast<std::uintptr_t>(column.data()) % 64 == 0;
    });
    EXPECT_TRUE(columns_aligned);

    clock.completed_ticks = 1;
    telemetry_manager.tick();
    tick_series = telemetry_manager.materialize_tick_series();
    EXPECT_EQ(active_count_data.num(), 1);
    EXPECT_EQ(kill_count_data.num(), 1);
    EXPECT_EQ(tick_series.active_lasers.num(), 1);
    EXPECT_EQ(telemetry_manager.get_history_stats().used_sample_count, 1);

    clock.completed_ticks = 2;
    clock.tick_loop.time_scale = 4.0;
    telemetry_manager.tick();
    tick_series = telemetry_manager.materialize_tick_series();
    EXPECT_EQ(telemetry_manager.get_history_stats().used_sample_count, 1);

    clock.completed_ticks = 5;
    clock.tick_loop.tick_rate = 4.0;
    auto const laser_snapshot{telemetry_manager.make_snapshot()};
    EXPECT_DOUBLE_EQ(laser_snapshot.elapsed_seconds, 1.25);
    EXPECT_EQ(laser_snapshot.active_lasers, 0);
    EXPECT_EQ(laser_snapshot.lasers_fired, 0);
    EXPECT_EQ(laser_snapshot.active_entity_count_data.last_time(), 5);
    EXPECT_EQ(laser_snapshot.cumulative_kill_count_data.last_time(), 5);
    EXPECT_EQ(laser_snapshot.active_entity_count_data.last_value(), 0);
    EXPECT_EQ(laser_snapshot.cumulative_kill_count_data.last_value(), 0);

    telemetry_manager.reset();
    EXPECT_TRUE(active_count_data.is_empty());
    EXPECT_TRUE(kill_count_data.is_empty());
    EXPECT_EQ(telemetry_manager.get_current_state().active_lasers, 0);

    clock.initialise({});
    telemetry_manager.initialise();
    EXPECT_EQ(active_count_data.num(), 1);
    EXPECT_EQ(active_count_data.last_time(), 0);

    entity_ledger.record_spawn(EntityType::PlayerShip, Team::Green, true);
    entity_ledger.record_spawn(EntityType::CapitalShip, Team::Red, true);
    entity_ledger.record_spawn(EntityType::Fighter, Team::Red, true);

    clock.completed_ticks = 2;
    telemetry_manager.tick();
    tick_series = telemetry_manager.materialize_tick_series();
    EXPECT_EQ(active_count_data.last_value(), 3);
    EXPECT_EQ(active_count_data.last_time(), 2);
    auto const& entity_state{telemetry_manager.get_current_state()};
    EXPECT_EQ(entity_state.active_entities_by_team_and_type[std::to_underlying(
                  Team::Green)][std::to_underlying(EntityType::PlayerShip)],
              1);
    EXPECT_EQ(entity_state.active_entities_by_team_and_type[std::to_underlying(
                  Team::Red)][std::to_underlying(EntityType::CapitalShip)],
              1);
    EXPECT_EQ(entity_state.active_entities_by_team_and_type[std::to_underlying(
                  Team::Red)][std::to_underlying(EntityType::Fighter)],
              1);
    auto const green_index{std::to_underlying(Team::Green)};
    auto const player_ship_index{std::to_underlying(EntityType::PlayerShip)};
    auto const white_index{std::to_underlying(Team::White)};
    EXPECT_EQ(tick_series.active_entities_by_team_and_type[green_index][player_ship_index].num(),
              2);
    EXPECT_EQ(tick_series.active_entities_by_team_and_type[white_index][player_ship_index].num(),
              1);

    EXPECT_EQ(entity_state.spawned_entities, 3);
    auto const entity_change_columns{
        LevelTelemetryManagerTestAccess::history(telemetry_manager).block_view(0).columns()};
    EXPECT_EQ(entity_change_columns.num(), 2);
    EXPECT_EQ(std::popcount(entity_change_columns.validity_masks[1].value()), 8);

    clock.completed_ticks = 3;
    telemetry_manager.tick();
    EXPECT_EQ(active_count_data.num(), 2);

    clock.tick_loop.tick_rate = 2.0;
    auto const entity_snapshot{telemetry_manager.make_snapshot()};
    EXPECT_DOUBLE_EQ(entity_snapshot.elapsed_seconds, 1.5);
    EXPECT_EQ(entity_snapshot.spawned_entities, 3);
    EXPECT_EQ(entity_snapshot.active_entities, 3);
    EXPECT_EQ(entity_snapshot.destroyed_entities, 0);
    EXPECT_EQ(entity_snapshot.kills, 0);
    EXPECT_EQ(entity_snapshot.active_entity_count_data.last_value(), 3);
    EXPECT_EQ(entity_snapshot.cumulative_kill_count_data.last_value(), 0);
    for (auto const value : entity_snapshot.active_entity_count_data.values()) {
        EXPECT_GE(value, 0);
    }
    for (auto const value : entity_snapshot.cumulative_kill_count_data.values()) {
        EXPECT_GE(value, 0);
    }

    LevelTelemetryManager boundary_manager{
        clock,
        entity_ledger,
        lasers,
        game_memory,
        {.block_bytes = telemetry::HistoryRowsSingleLayout::layout_bytes(1)},
    };
    clock.initialise({});
    boundary_manager.initialise();
    EXPECT_EQ(boundary_manager.get_history_stats().total_sample_capacity, 64);
    for (std::uint64_t tick{1}; tick <= 64; ++tick) {
        clock.completed_ticks = tick;
        entity_ledger.record_spawn(EntityType::CapitalShip, Team::Red, true);
        boundary_manager.tick();
    }
    EXPECT_EQ(boundary_manager.get_history_stats().used_sample_count, 65);
    EXPECT_EQ(boundary_manager.get_history_stats().acquired_block_count, 2);
    EXPECT_EQ(boundary_manager.materialize_tick_series().active_entities.num(), 65);
    auto const grown_capacity{boundary_manager.get_history_stats().total_sample_capacity};
    boundary_manager.reset();
    EXPECT_EQ(boundary_manager.get_history_stats().used_sample_count, 0);
    EXPECT_EQ(boundary_manager.get_history_stats().total_sample_capacity, grown_capacity);
}

} // namespace ioj::sim::tests
