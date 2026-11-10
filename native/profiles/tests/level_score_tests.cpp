#include <ioj/profiles/level_score.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/sim/levels/reference_level_simulation_data.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <gtest/gtest.h>

namespace ioj::profiles {
TEST(LevelScore, QueriesRetainedPlayerResultWithoutSideEffects) {
    auto const definition{levels::LevelDefinition::make(
        {.metadata = {.id = levels::LevelId{"publication"},
                      .title = "Publication",
                      .par_time_seconds = 1.f},
         .player_entity_id = levels::EntityId{"player"},
         .logic = {.expressions = {{levels::TimerElapsedExpr{0}}},
                   .timer_count = 1,
                   .initial_effects = {levels::StartTimerEffect{0, NonNegativeFinite<double>{0.2}}},
                   .rules = {{0, {levels::WinEffect{Team::Blue}}}}},
         .entities = {{.id = levels::EntityId{"player"},
                       .archetype = levels::EntityArchetype::PlayerFighter,
                       .team = Team::Blue,
                       .position = {0., 30000., 0.}},
                      {.id = levels::EntityId{"capital"},
                       .archetype = levels::EntityArchetype::CapitalShip,
                       .team = Team::Red},
                      {.id = levels::EntityId{"turret"},
                       .archetype = levels::EntityArchetype::StaticTurret,
                       .team = Team::Red,
                       .position = {20000., 0., 0.}}}})};
    ASSERT_TRUE(definition);
    auto geometry{sim::make_reference_level_simulation_data()};
    geometry.data.player = geometry.player;
    geometry.data.collision.grid_geometry.dimensions = {16, 16, 4};
    geometry.data.collision.grid_geometry.cell_dimensions = {10000.f, 10000.f, 10000.f};
    geometry.data.tick_memory_capacity_bytes = 4 * 1024 * 1024;
    geometry.data.phase_memory_capacity_bytes = 4 * 1024 * 1024;
    geometry.data.clock_settings.tick_rate = Frequency::make(10.);
    auto built{sim::make_level_simulation_init_data(*definition, std::move(geometry.data))};
    ASSERT_TRUE(built) << built.error().front();
    sim::LevelSim simulation{std::move(*built)};
    EXPECT_FALSE(make_level_score(simulation, 17));

    auto const player{simulation.player_ship_simulation()->unique_entity_id};
    sim::DirectDamageEvents damage;
    damage.add(simulation.capital_ships().get_entities().entity_ids()[0], 100000, player);
    damage.add(simulation.turrets().get_entities().entity_ids()[0], 100000, player);
    sim::LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.start();
    simulation.advance(0.4);
    auto const record{make_level_score(simulation, 17)};
    ASSERT_TRUE(record);
    ScoreRecord const expected{.date = 17,
                               .level_name = "publication",
                               .team = Team::Blue,
                               .outcome = LevelOutcome::Victory,
                               .end_reason = sim::RunEndReason::LevelCompleted,
                               .kills = 2,
                               .time_seconds = 0.2f};
    EXPECT_EQ(*record, expected);
    EXPECT_EQ(make_level_score(simulation, 17), record);
    EXPECT_TRUE(simulation.result());
}
}
