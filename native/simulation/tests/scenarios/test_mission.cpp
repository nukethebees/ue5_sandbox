#include "../support/mission_test_support.h"

namespace ioj::sim::tests {
TEST(WorldlessSpaceGameSimulation, KillThresholdWaitsForFinalCreditedDeath) {
    LevelSimInitData data;
    data.collision.grid_geometry = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.capital_ships.fighter_spawn_slots = 0;
    auto const hero{add_capital_spawn(data, {{-2000.f, 0, 0}}, Team::Green, 60.f, 60.f)};
    add_capital_spawn(data, {{2000.f, 0, 0}}, Team::Red, 60.f, 60.f);
    add_capital_spawn(data, {{4000.f, 0, 0}}, Team::Red, 60.f, 60.f);
    levels::ObjectiveDefinition objective{.team = Team::Green};
    objective.expressions = {
        {levels::EntityKillsReachedExpr{.entities = {mission_entity(hero)}, .kills = 2}}};
    objective.roots[ObjectiveOutcome::Passed] = 0;
    install_mission(data, std::move(objective));

    WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.simulation()};
    auto const ids{simulation.capital_ships().get_entities().entity_ids()};
    auto const killer{ids[0]};
    auto const first{ids[1]};
    auto const last{ids[2]};
    auto const period{simulation.clock().tick_period()};
    simulation.start();
    harness.advance(period);
    harness.queue_kills(std::array{first}, killer);
    harness.advance(period);
    EXPECT_EQ(simulation.state(), LevelState::Running);
    EXPECT_FALSE(simulation.mission().mission_completed());

    harness.queue_kills(std::array{last}, killer);
    harness.advance(period);
    EXPECT_EQ(simulation.entity_ledger().kills(killer), 2);
    EXPECT_EQ(simulation.mission().outcome(ObjectiveId{0}, simulation.expressions()),
              ObjectiveOutcome::Passed);
    EXPECT_EQ(simulation.state(), LevelState::Paused);
}
}
