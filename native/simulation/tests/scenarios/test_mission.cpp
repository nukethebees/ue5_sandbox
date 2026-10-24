#include "../support/mission_test_support.h"
#include "../support/simulation_test_support.h"
#include <ioj/sim/testing/entity_observations.h>

namespace ioj::sim {
enum class MissionScenario : std::uint8_t {
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
    DefenceObjective,
    RequiredKillsObjective,
    RequiredKillsTimeElapsed,
    AuthoredKillTarget,
    SuccessIsTerminal,
    OutcomeCompletionIsLatched,
};

void run_worldless_mission_scenario(tests::SimulationFixture const& config,
                                    MissionScenario const scenario) {
    using Scenario = MissionScenario;
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const hero{tests::add_capital_spawn(
        data, {{-2000, 0, 0}}, Team::Green, invalid_level_entity_index, 60.f, 60.f)};
    std::optional<std::uint32_t> ordinary;
    std::optional<std::uint32_t> required;
    std::optional<std::uint32_t> second;
    auto const needs_kills{
        scenario == Scenario::KillEnemies || scenario == Scenario::KillEnemiesWithinTime ||
        scenario == Scenario::RequiredKillsObjective || scenario == Scenario::AuthoredKillTarget};
    if (needs_kills) {
        ordinary = tests::add_capital_spawn(
            data, {{2000, 0, 0}}, Team::Red, invalid_level_entity_index, 60.f, 60.f);
    }
    if (scenario == Scenario::RequiredKillsObjective ||
        scenario == Scenario::RequiredKillsTimeElapsed) {
        required = tests::add_capital_spawn(
            data, {{4000, 0, 0}}, Team::Red, invalid_level_entity_index, 60.f, 60.f);
    }
    if (scenario == Scenario::AuthoredKillTarget) {
        second = tests::add_capital_spawn(
            data, {{4000, 0, 0}}, Team::Red, invalid_level_entity_index, 60.f, 60.f);
    }

    levels::ObjectiveDefinition objective{.team = Team::Green};
    auto add = [&](levels::MissionExpression expression) {
        auto const index{static_cast<std::uint32_t>(objective.expressions.size())};
        objective.expressions.push_back(std::move(expression));
        return index;
    };
    std::vector<std::uint32_t> pass;
    std::vector<std::uint32_t> fail;
    if (needs_kills) {
        pass.push_back(add({ioj::levels::EntityKillsReachedExpression{
            .entities = {tests::mission_entity(hero)},
            .kills = scenario == Scenario::AuthoredKillTarget ? 2u : 1u}}));
    } else {
        auto const seconds{scenario == Scenario::DefenceObjective             ? 10.0
                           : scenario == Scenario::OutcomeCompletionIsLatched ? 0.01
                                                                              : 0.1};
        pass.push_back(add({ioj::levels::TimeElapsedExpression{
            .seconds = ioj::NonNegativeFinite<double>::make(seconds)}}));
    }
    if (required) {
        pass.push_back(add(tests::destroyed(*required)));
    }
    fail.push_back(add(tests::destroyed(hero)));
    if (scenario == Scenario::KillEnemiesWithinTime ||
        scenario == Scenario::RequiredKillsTimeElapsed) {
        fail.push_back(add({ioj::levels::TimeElapsedExpression{
            .seconds = ioj::NonNegativeFinite<double>::make(0.1)}}));
    }
    objective.roots[ioj::ObjectiveOutcome::Passed] =
        add({ioj::levels::AllExpression{.children = std::move(pass)}});
    objective.roots[ioj::ObjectiveOutcome::Failed] =
        add({ioj::levels::AnyExpression{.children = std::move(fail)}});
    tests::install_mission(data, std::move(objective));

    tests::WorldlessSimulationTest harness{std::move(data)};
    auto& simulation{harness.get_simulation()};
    auto const& capitals{simulation.get_capital_ships()};
    auto const hero_id{capitals.get_id(0)};
    auto const ordinary_id{ordinary ? capitals.get_id(1) : EntityId{}};
    auto const required_id{required ? capitals.get_id(ordinary ? 2 : 1) : EntityId{}};
    auto const second_id{second ? capitals.get_id(2) : EntityId{}};
    harness.finish_initialisation();

    if (scenario == Scenario::KillEnemies) {
        harness.timeline.then_after(0.01,
                                    [&] { harness.queue_kills(std::array{ordinary_id}, hero_id); });
    } else if (scenario == Scenario::DefenceObjective) {
        harness.timeline.then_after(0.01, [&] { harness.queue_kills(std::array{hero_id}); });
    } else if (scenario == Scenario::RequiredKillsObjective) {
        harness.timeline
            .then_after(0.01, [&] { harness.queue_kills(std::array{ordinary_id}, hero_id); })
            .then_after(0.19, [&] { harness.queue_kills(std::array{required_id}); });
    } else if (scenario == Scenario::AuthoredKillTarget) {
        harness.timeline
            .then_after(0.01, [&] { harness.queue_kills(std::array{ordinary_id}, hero_id); })
            .then_after(0.19, [&] { harness.queue_kills(std::array{second_id}, hero_id); });
    } else if (scenario == Scenario::SuccessIsTerminal ||
               scenario == Scenario::OutcomeCompletionIsLatched) {
        harness.timeline.at(0.15, [&] { harness.queue_kills(std::array{hero_id}); });
    }
    auto gated{false};
    harness.on_end_tick = [&](LevelSim&) {
        if (simulation.get_clock().get_simulation_time() >= 0.1 &&
            !simulation.get_mission().outcome(ioj::sim::ObjectiveId{0})) {
            gated = true;
        }
    };
    harness.timeline.finish_at(0.3);
    EXPECT_FALSE(simulation.get_mission().outcome(ioj::sim::ObjectiveId{0}));
    ASSERT_TRUE(harness.run_until_timeline_finished(1.0));
    auto const failed{scenario == Scenario::DefenceObjective ||
                      scenario == Scenario::KillEnemiesWithinTime ||
                      scenario == Scenario::RequiredKillsTimeElapsed};
    EXPECT_EQ(simulation.get_mission().outcome(ioj::sim::ObjectiveId{0}),
              failed ? ObjectiveOutcome::Failed : ObjectiveOutcome::Passed);
    EXPECT_EQ(simulation.get_mission().team_outcome(Team::Green),
              failed ? TeamOutcome::Defeat : TeamOutcome::Victory);
    EXPECT_EQ(simulation.get_mission().completed_objectives().size(), 1);
    EXPECT_TRUE(simulation.get_mission().active_rules().empty());
    if (scenario == Scenario::RequiredKillsObjective || scenario == Scenario::AuthoredKillTarget) {
        EXPECT_TRUE(gated);
        EXPECT_EQ(simulation.get_entity_ledger().get_kills(hero_id),
                  scenario == Scenario::AuthoredKillTarget ? 2u : 1u);
    }
}
TEST(WorldlessSpaceGameSimulation, Mission_SurviveTime) {
    run_worldless_mission_scenario(tests::make_fixture(), MissionScenario::SurviveTime);
}

TEST(WorldlessSpaceGameSimulation, Mission_KillEnemies) {
    run_worldless_mission_scenario(tests::make_fixture(), MissionScenario::KillEnemies);
}

TEST(WorldlessSpaceGameSimulation, Mission_KillEnemiesWithinTime) {
    run_worldless_mission_scenario(tests::make_fixture(), MissionScenario::KillEnemiesWithinTime);
}

TEST(WorldlessSpaceGameSimulation, Mission_DefenceObjective) {
    run_worldless_mission_scenario(tests::make_fixture(), MissionScenario::DefenceObjective);
}

TEST(WorldlessSpaceGameSimulation, Mission_RequiredKillsObjective) {
    run_worldless_mission_scenario(tests::make_fixture(), MissionScenario::RequiredKillsObjective);
}

TEST(WorldlessSpaceGameSimulation, Mission_RequiredKillsTimeElapsed) {
    run_worldless_mission_scenario(tests::make_fixture(),
                                   MissionScenario::RequiredKillsTimeElapsed);
}

TEST(WorldlessSpaceGameSimulation, Mission_AutomaticKillTargetIncludesLastEnemy) {
    run_worldless_mission_scenario(tests::make_fixture(), MissionScenario::AuthoredKillTarget);
}

TEST(WorldlessSpaceGameSimulation, Mission_SuccessIsTerminal) {
    run_worldless_mission_scenario(tests::make_fixture(), MissionScenario::SuccessIsTerminal);
}

TEST(WorldlessSpaceGameSimulation, Mission_ExplicitCompletionIsLatched) {
    run_worldless_mission_scenario(tests::make_fixture(),
                                   MissionScenario::OutcomeCompletionIsLatched);
}

}
