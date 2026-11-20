#include <ioj/profiles/level_score.h>
#include <ioj/sim/levels/level_simulation_builder.h>
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
    auto built{sim::make_level_simulation_init_data(*definition, {}, {})};
    ASSERT_TRUE(built) << built.error().front();
    sim::LevelSim simulation{std::move(*built)};
    EXPECT_FALSE(completed_level(simulation));

    auto const player{simulation.player_ship_simulation()->unique_entity_id};
    sim::DirectDamageEvents damage;
    damage.add(simulation.capital_ships().get_entities().entity_ids()[0], 100000, player);
    damage.add(simulation.turrets().get_entities().entity_ids()[0], 100000, player);
    sim::LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.start();
    simulation.advance(0.4);
    auto const completed{completed_level(simulation)};
    ASSERT_TRUE(completed);
    auto const record{make_level_score(*completed, 17)};
    ASSERT_TRUE(record);
    ScoreRecord const expected{.date = 17,
                               .level_name = "publication",
                               .team = Team::Blue,
                               .outcome = LevelOutcome::Victory,
                               .end_reason = sim::RunEndReason::LevelCompleted,
                               .kills = 2,
                               .time_seconds = 0.2f};
    EXPECT_EQ(*record, expected);
    EXPECT_EQ(make_level_score(*completed, 17), record);
    EXPECT_TRUE(simulation.result());
}
}
