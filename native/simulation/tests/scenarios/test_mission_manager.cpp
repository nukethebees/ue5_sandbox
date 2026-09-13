#include "../support/simulation_test_support.h"

#include "test_mission_manager.h"

#include <sandbox/simulation/missions/TestMissionManager.h>
#include <sandbox/simulation/ships/capital/TestCapitalShipsSimulation.h>

namespace ml {
namespace {
auto add_worldless_capital(FLevelSimulationInitData& data,
                           ml::Vector3d const location,
                           ml::simulation::Team const team = ml::simulation::Team::White)
    -> std::int32_t {
    auto const index{data.capital_spawns.num()};
    data.capital_spawns.add_defaulted(1);
    data.capital_spawns.locations.set(index, ml::simulation::to_float(location));
    data.capital_spawns.teams[index] = static_cast<ml::simulation::Team>(team);
    data.capital_spawns.healths[index] = data.capital_ships.max_health;
    data.capital_spawns.initial_spawn_delays[index] = 60.f;
    data.capital_spawns.spawn_cooldowns[index] = 60.f;
    return index;
}
}

void run_worldless_mission_manager_scenario(ml::simulation_tests::SimulationFixture const& config,
                                            EMissionManagerScenario const scenario) {
    using EScenario = EMissionManagerScenario;

    auto data{ml::simulation_tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const hero_index{add_worldless_capital(data,
                                                ml::Vector3d{-2000.f, 0.f, 0.f},
                                                scenario == EScenario::AutomaticKillTarget
                                                    ? ml::simulation::Team::Green
                                                    : ml::simulation::Team::White)};
    std::int32_t ordinary_enemy_index{-1};
    std::int32_t required_enemy_index{-1};
    if (scenario == EScenario::KillEnemies || scenario == EScenario::KillEnemiesWithinTime) {
        ordinary_enemy_index = add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f});
    } else if (scenario == EScenario::RequiredKillsObjective) {
        ordinary_enemy_index = add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f});
        required_enemy_index = add_worldless_capital(data, ml::Vector3d{4000.f, 0.f, 0.f});
    } else if (scenario == EScenario::RequiredKillsTimeElapsed) {
        required_enemy_index = add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f});
    } else if (scenario == EScenario::AutomaticKillTarget) {
        ordinary_enemy_index =
            add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f}, ml::simulation::Team::Red);
        add_worldless_capital(data, ml::Vector3d{4000.f, 0.f, 0.f}, ml::simulation::Team::Red);
    }

    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    auto& simulation{harness.get_simulation()};
    auto& manager{simulation.get_mission_manager()};
    auto const& capitals{simulation.get_capital_ships()};
    auto const hero{capitals.get_handle(hero_index)};
    auto const ordinary_enemy{ordinary_enemy_index == -1
                                  ? FRegistryEntityHandle{}
                                  : capitals.get_handle(ordinary_enemy_index)};
    auto const required_enemy{required_enemy_index == -1
                                  ? FRegistryEntityHandle{}
                                  : capitals.get_handle(required_enemy_index)};

    manager.set_save_mission_results(false);
    switch (scenario) {
        case EScenario::SurviveTime:
            manager.set_mission_mode(ml::simulation::MissionMode::SurviveTime);
            manager.set_target_time(0.1f);
            manager.add_entity_that_must_survive(hero);
            break;
        case EScenario::KillEnemies:
            manager.set_mission_mode(ml::simulation::MissionMode::KillEnemies);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            break;
        case EScenario::KillEnemiesWithinTime:
            manager.set_mission_mode(ml::simulation::MissionMode::KillEnemiesWithinTime);
            manager.set_target_time(0.1f);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            break;
        case EScenario::DefenceObjective:
        case EScenario::SuccessIsTerminal:
        case EScenario::ExplicitCompletionIsLatched:
            manager.set_mission_mode(ml::simulation::MissionMode::SurviveTime);
            manager.set_target_time(scenario == EScenario::SuccessIsTerminal ? 0.1f : 10.f);
            manager.add_entity_that_must_survive(hero);
            break;
        case EScenario::RequiredKillsObjective:
            manager.set_mission_mode(ml::simulation::MissionMode::KillEnemies);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            manager.add_entity_required_to_kill(required_enemy);
            break;
        case EScenario::RequiredKillsTimeElapsed:
            manager.set_mission_mode(ml::simulation::MissionMode::SurviveTime);
            manager.set_target_time(0.1f);
            manager.add_entity_that_must_survive(hero);
            manager.add_entity_required_to_kill(required_enemy);
            break;
        case EScenario::AutomaticKillTarget:
            manager.set_mission_mode(ml::simulation::MissionMode::KillEnemies);
            manager.set_kill_target(0);
            manager.add_hero_entity(hero);
            break;
        default:
            assert(false && "unreachable scenario");
            break;
    }
    harness.finish_initialisation();

    struct Sample {
        ml::simulation::MissionState state{ml::simulation::MissionState::NotStarted};
        ml::simulation::MissionFailReason fail_reason{ml::simulation::MissionFailReason::None};
        std::int32_t kills{};
        std::int32_t kill_target{};
        bool survivor_alive{};
        std::int32_t survivor_health{};
        std::int32_t required_health{};
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample;
        sample.state = manager.get_mission_state();
        sample.fail_reason = manager.get_mission_fail_reason();
        sample.kills = manager.get_mission_kills();
        sample.kill_target = manager.get_kill_target();
        auto const survivors{manager.get_entity_handles_that_must_survive()};
        sample.survivor_alive =
            !survivors.empty() && harness.get_registry().is_valid_alive(survivors[0]);
        auto const survivor_health{manager.get_entity_health_that_must_survive()};
        sample.survivor_health = survivor_health.empty() ? 0 : survivor_health[0].health;
        auto const required_health{manager.get_entity_health_required_to_kill()};
        sample.required_health = required_health.empty() ? 0 : required_health[0].health;
        samples.add(harness.get_time(), sample);
    };

    auto first_completion_result{false};
    auto duplicate_completion_result{true};
    if (scenario == EScenario::KillEnemies) {
        harness.timeline.then_after(0.01,
                                    [&] { harness.queue_kills(std::array{ordinary_enemy}, hero); });
    } else if (scenario == EScenario::DefenceObjective) {
        harness.timeline.then_after(0.01, [&] { harness.queue_kills(std::array{hero}); });
    } else if (scenario == EScenario::RequiredKillsObjective) {
        harness.timeline
            .then_after(0.01, [&] { harness.queue_kills(std::array{ordinary_enemy}, hero); })
            .then_after(0.19, [&] { harness.queue_kills(std::array{required_enemy}); });
    } else if (scenario == EScenario::AutomaticKillTarget) {
        harness.timeline
            .then_after(0.01, [&] { harness.queue_kills(std::array{ordinary_enemy}, hero); })
            .then_after(0.19, [&] {
                auto const second_enemy{simulation.get_capital_ships().get_handle(1)};
                harness.queue_kills(std::array{second_enemy}, hero);
            });
    } else if (scenario == EScenario::SuccessIsTerminal) {
        harness.timeline.at(0.15, [&] { harness.queue_kills(std::array{hero}); });
    } else if (scenario == EScenario::ExplicitCompletionIsLatched) {
        harness.timeline.then_after(0.01, [&] {
            first_completion_result = manager.complete_mission();
            duplicate_completion_result = manager.complete_mission();
        });
    }
    auto const end_time{scenario == EScenario::RequiredKillsObjective ||
                                scenario == EScenario::AutomaticKillTarget
                            ? 0.3
                            : 0.25};
    harness.timeline.finish_at(end_time);

    ml::simulation_tests::expect_equal(manager.get_mission_state(),
                                       ml::simulation::MissionState::Running,
                                       "Mission starts running");
    ml::simulation_tests::expect_false(manager.should_save_mission_results(),
                                       "Mission result saving is disabled");
    ml::simulation_tests::expect_true(
        harness.run_until_timeline_finished(1.0),
        "Mission timeline completes within its simulation-time limit");
    ml::simulation_tests::expect_true(!samples.is_empty(), "Mission simulation samples recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& final{samples.last_value()};
    switch (scenario) {
        case EScenario::SurviveTime:
            ml::simulation_tests::expect_equal(final.state,
                                               ml::simulation::MissionState::Succeeded,
                                               "Survive-time mission succeeds");
            ml::simulation_tests::expect_equal(final.fail_reason,
                                               ml::simulation::MissionFailReason::None,
                                               "Successful mission has no failure reason");
            break;
        case EScenario::KillEnemies:
            ml::simulation_tests::expect_equal(
                final.state, ml::simulation::MissionState::Succeeded, "Kill mission succeeds");
            ml::simulation_tests::expect_equal(final.kills, 1, "Hero kill contributes to mission");
            break;
        case EScenario::KillEnemiesWithinTime:
            ml::simulation_tests::expect_equal(
                final.state, ml::simulation::MissionState::Failed, "Timed kill mission fails");
            ml::simulation_tests::expect_equal(final.fail_reason,
                                               ml::simulation::MissionFailReason::TimeElapsed,
                                               "Timed mission reports elapsed time");
            break;
        case EScenario::DefenceObjective:
            ml::simulation_tests::expect_equal(final.state,
                                               ml::simulation::MissionState::Failed,
                                               "Defence objective failure fails mission");
            ml::simulation_tests::expect_equal(
                final.fail_reason,
                ml::simulation::MissionFailReason::DefenceObjectiveFailed,
                "Defence failure reason is retained");
            ml::simulation_tests::expect_equal(
                final.survivor_health, 0, "Destroyed defence objective reports zero health");
            break;
        case EScenario::RequiredKillsObjective: {
            auto const& gated{samples.nearest_value(0.1)};
            ml::simulation_tests::expect_equal(gated.state,
                                               ml::simulation::MissionState::Running,
                                               "Normal kill target does not bypass required kill");
            ml::simulation_tests::expect_equal(
                gated.kills, 1, "Normal kill target is met before required kill");
            ml::simulation_tests::expect_true(
                gated.required_health > 0,
                "Required target remains healthy while mission is gated");
            ml::simulation_tests::expect_equal(final.state,
                                               ml::simulation::MissionState::Succeeded,
                                               "Required-kill mission succeeds");
            ml::simulation_tests::expect_equal(
                final.kills, 1, "Uncredited required kill preserves mission kills");
            ml::simulation_tests::expect_equal(
                final.required_health, 0, "Destroyed required target reports zero health");
            break;
        }
        case EScenario::RequiredKillsTimeElapsed:
            ml::simulation_tests::expect_equal(
                final.state,
                ml::simulation::MissionState::Failed,
                "Incomplete required kill fails survive-time mission");
            ml::simulation_tests::expect_equal(final.fail_reason,
                                               ml::simulation::MissionFailReason::TimeElapsed,
                                               "Incomplete required kill reports elapsed time");
            ml::simulation_tests::expect_true(final.required_health > 0,
                                              "Required target remains alive at timeout");
            break;
        case EScenario::AutomaticKillTarget: {
            auto const& one_remaining{samples.nearest_value(0.1)};
            ml::simulation_tests::expect_equal(
                one_remaining.kill_target, 2, "Automatic target counts both initial enemies");
            ml::simulation_tests::expect_equal(one_remaining.state,
                                               ml::simulation::MissionState::Running,
                                               "Mission remains running with one enemy left");
            ml::simulation_tests::expect_equal(
                one_remaining.kills, 1, "First enemy kill is credited");
            ml::simulation_tests::expect_equal(final.state,
                                               ml::simulation::MissionState::Succeeded,
                                               "Last enemy completes automatic kill target");
            ml::simulation_tests::expect_equal(final.kills, 2, "Both enemy kills are credited");
            break;
        }
        case EScenario::SuccessIsTerminal:
            ml::simulation_tests::expect_equal(
                final.state,
                ml::simulation::MissionState::Succeeded,
                "Mission remains successful after later destruction");
            ml::simulation_tests::expect_equal(final.fail_reason,
                                               ml::simulation::MissionFailReason::None,
                                               "Later destruction does not add a failure reason");
            ml::simulation_tests::expect_false(final.survivor_alive,
                                               "Defended entity is destroyed after success");
            break;
        case EScenario::ExplicitCompletionIsLatched:
            ml::simulation_tests::expect_true(first_completion_result,
                                              "Explicit completion performs the state transition");
            ml::simulation_tests::expect_false(duplicate_completion_result,
                                               "Duplicate completion is ignored");
            ml::simulation_tests::expect_equal(final.state,
                                               ml::simulation::MissionState::Succeeded,
                                               "Explicit completion leaves the mission succeeded");
            break;
        default:
            assert(false && "unreachable scenario");
            break;
    }
}

}
