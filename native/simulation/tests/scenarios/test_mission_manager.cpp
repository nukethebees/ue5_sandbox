#include "../support/simulation_test_support.h"

#include "test_mission_manager.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/mission_manager.h>

namespace ioj::sim {
namespace {
auto add_worldless_capital(LevelSimInitData& data,
                           ml::Vector3d const location,
                           ioj::sim::Team const team = ioj::sim::Team::White) -> std::int32_t {
    auto const index{data.capital_spawns.num()};
    data.capital_spawns.add_defaulted(1);
    data.capital_spawns.locations.set(index, ioj::sim::to_float(location));
    data.capital_spawns.teams[index] = static_cast<ioj::sim::Team>(team);
    data.capital_spawns.healths[index] = data.capital_ships.max_health;
    data.capital_spawns.initial_spawn_delays[index] = 60.f;
    data.capital_spawns.spawn_cooldowns[index] = 60.f;
    return index;
}
}

void run_worldless_mission_manager_scenario(ioj::sim::tests::SimulationFixture const& config,
                                            MissionManagerScenario const scenario) {
    using Scenario = MissionManagerScenario;

    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const hero_index{add_worldless_capital(
        data,
        ml::Vector3d{-2000.f, 0.f, 0.f},
        scenario == Scenario::AutomaticKillTarget ? ioj::sim::Team::Green : ioj::sim::Team::White)};
    std::int32_t ordinary_enemy_index{-1};
    std::int32_t required_enemy_index{-1};
    if (scenario == Scenario::KillEnemies || scenario == Scenario::KillEnemiesWithinTime) {
        ordinary_enemy_index = add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f});
    } else if (scenario == Scenario::RequiredKillsObjective) {
        ordinary_enemy_index = add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f});
        required_enemy_index = add_worldless_capital(data, ml::Vector3d{4000.f, 0.f, 0.f});
    } else if (scenario == Scenario::RequiredKillsTimeElapsed) {
        required_enemy_index = add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f});
    } else if (scenario == Scenario::AutomaticKillTarget) {
        ordinary_enemy_index =
            add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f}, ioj::sim::Team::Red);
        add_worldless_capital(data, ml::Vector3d{4000.f, 0.f, 0.f}, ioj::sim::Team::Red);
    }

    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    auto& simulation{harness.get_simulation()};
    auto& manager{simulation.get_mission_manager()};
    auto const& capitals{simulation.get_capital_ships()};
    auto const hero{capitals.get_handle(hero_index)};
    auto const ordinary_enemy{ordinary_enemy_index == -1
                                  ? RegistryEntityHandle{}
                                  : capitals.get_handle(ordinary_enemy_index)};
    auto const required_enemy{required_enemy_index == -1
                                  ? RegistryEntityHandle{}
                                  : capitals.get_handle(required_enemy_index)};

    manager.set_save_mission_results(false);
    switch (scenario) {
        case Scenario::SurviveTime:
            manager.set_mission_mode(ioj::sim::MissionMode::SurviveTime);
            manager.set_target_time(0.1f);
            manager.add_entity_that_must_survive(hero);
            break;
        case Scenario::KillEnemies:
            manager.set_mission_mode(ioj::sim::MissionMode::KillEnemies);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            break;
        case Scenario::KillEnemiesWithinTime:
            manager.set_mission_mode(ioj::sim::MissionMode::KillEnemiesWithinTime);
            manager.set_target_time(0.1f);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            break;
        case Scenario::DefenceObjective:
        case Scenario::SuccessIsTerminal:
        case Scenario::ExplicitCompletionIsLatched:
            manager.set_mission_mode(ioj::sim::MissionMode::SurviveTime);
            manager.set_target_time(scenario == Scenario::SuccessIsTerminal ? 0.1f : 10.f);
            manager.add_entity_that_must_survive(hero);
            break;
        case Scenario::RequiredKillsObjective:
            manager.set_mission_mode(ioj::sim::MissionMode::KillEnemies);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            manager.add_entity_required_to_kill(required_enemy);
            break;
        case Scenario::RequiredKillsTimeElapsed:
            manager.set_mission_mode(ioj::sim::MissionMode::SurviveTime);
            manager.set_target_time(0.1f);
            manager.add_entity_that_must_survive(hero);
            manager.add_entity_required_to_kill(required_enemy);
            break;
        case Scenario::AutomaticKillTarget:
            manager.set_mission_mode(ioj::sim::MissionMode::KillEnemies);
            manager.set_kill_target(0);
            manager.add_hero_entity(hero);
            break;
        default:
            assert(false && "unreachable scenario");
            break;
    }
    harness.finish_initialisation();

    struct Sample {
        ioj::sim::MissionState state{ioj::sim::MissionState::NotStarted};
        ioj::sim::MissionFailReason fail_reason{ioj::sim::MissionFailReason::None};
        std::int32_t kills{};
        std::int32_t kill_target{};
        bool survivor_alive{};
        std::int32_t survivor_health{};
        std::int32_t required_health{};
    };
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
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
    if (scenario == Scenario::KillEnemies) {
        harness.timeline.then_after(0.01,
                                    [&] { harness.queue_kills(std::array{ordinary_enemy}, hero); });
    } else if (scenario == Scenario::DefenceObjective) {
        harness.timeline.then_after(0.01, [&] { harness.queue_kills(std::array{hero}); });
    } else if (scenario == Scenario::RequiredKillsObjective) {
        harness.timeline
            .then_after(0.01, [&] { harness.queue_kills(std::array{ordinary_enemy}, hero); })
            .then_after(0.19, [&] { harness.queue_kills(std::array{required_enemy}); });
    } else if (scenario == Scenario::AutomaticKillTarget) {
        harness.timeline
            .then_after(0.01, [&] { harness.queue_kills(std::array{ordinary_enemy}, hero); })
            .then_after(0.19, [&] {
                auto const second_enemy{simulation.get_capital_ships().get_handle(1)};
                harness.queue_kills(std::array{second_enemy}, hero);
            });
    } else if (scenario == Scenario::SuccessIsTerminal) {
        harness.timeline.at(0.15, [&] { harness.queue_kills(std::array{hero}); });
    } else if (scenario == Scenario::ExplicitCompletionIsLatched) {
        harness.timeline.then_after(0.01, [&] {
            first_completion_result = manager.complete_mission();
            duplicate_completion_result = manager.complete_mission();
        });
    }
    auto const end_time{scenario == Scenario::RequiredKillsObjective ||
                                scenario == Scenario::AutomaticKillTarget
                            ? 0.3
                            : 0.25};
    harness.timeline.finish_at(end_time);

    ioj::sim::tests::expect_equal(
        manager.get_mission_state(), ioj::sim::MissionState::Running, "Mission starts running");
    ioj::sim::tests::expect_false(manager.should_save_mission_results(),
                                  "Mission result saving is disabled");
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(1.0),
                                 "Mission timeline completes within its simulation-time limit");
    ioj::sim::tests::expect_true(!samples.is_empty(), "Mission simulation samples recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& final{samples.last_value()};
    switch (scenario) {
        case Scenario::SurviveTime:
            ioj::sim::tests::expect_equal(
                final.state, ioj::sim::MissionState::Succeeded, "Survive-time mission succeeds");
            ioj::sim::tests::expect_equal(final.fail_reason,
                                          ioj::sim::MissionFailReason::None,
                                          "Successful mission has no failure reason");
            break;
        case Scenario::KillEnemies:
            ioj::sim::tests::expect_equal(
                final.state, ioj::sim::MissionState::Succeeded, "Kill mission succeeds");
            ioj::sim::tests::expect_equal(final.kills, 1, "Hero kill contributes to mission");
            break;
        case Scenario::KillEnemiesWithinTime:
            ioj::sim::tests::expect_equal(
                final.state, ioj::sim::MissionState::Failed, "Timed kill mission fails");
            ioj::sim::tests::expect_equal(final.fail_reason,
                                          ioj::sim::MissionFailReason::TimeElapsed,
                                          "Timed mission reports elapsed time");
            break;
        case Scenario::DefenceObjective:
            ioj::sim::tests::expect_equal(final.state,
                                          ioj::sim::MissionState::Failed,
                                          "Defence objective failure fails mission");
            ioj::sim::tests::expect_equal(final.fail_reason,
                                          ioj::sim::MissionFailReason::DefenceObjectiveFailed,
                                          "Defence failure reason is retained");
            ioj::sim::tests::expect_equal(
                final.survivor_health, 0, "Destroyed defence objective reports zero health");
            break;
        case Scenario::RequiredKillsObjective: {
            auto const& gated{samples.nearest_value(0.1)};
            ioj::sim::tests::expect_equal(gated.state,
                                          ioj::sim::MissionState::Running,
                                          "Normal kill target does not bypass required kill");
            ioj::sim::tests::expect_equal(
                gated.kills, 1, "Normal kill target is met before required kill");
            ioj::sim::tests::expect_true(gated.required_health > 0,
                                         "Required target remains healthy while mission is gated");
            ioj::sim::tests::expect_equal(
                final.state, ioj::sim::MissionState::Succeeded, "Required-kill mission succeeds");
            ioj::sim::tests::expect_equal(
                final.kills, 1, "Uncredited required kill preserves mission kills");
            ioj::sim::tests::expect_equal(
                final.required_health, 0, "Destroyed required target reports zero health");
            break;
        }
        case Scenario::RequiredKillsTimeElapsed:
            ioj::sim::tests::expect_equal(final.state,
                                          ioj::sim::MissionState::Failed,
                                          "Incomplete required kill fails survive-time mission");
            ioj::sim::tests::expect_equal(final.fail_reason,
                                          ioj::sim::MissionFailReason::TimeElapsed,
                                          "Incomplete required kill reports elapsed time");
            ioj::sim::tests::expect_true(final.required_health > 0,
                                         "Required target remains alive at timeout");
            break;
        case Scenario::AutomaticKillTarget: {
            auto const& one_remaining{samples.nearest_value(0.1)};
            ioj::sim::tests::expect_equal(
                one_remaining.kill_target, 2, "Automatic target counts both initial enemies");
            ioj::sim::tests::expect_equal(one_remaining.state,
                                          ioj::sim::MissionState::Running,
                                          "Mission remains running with one enemy left");
            ioj::sim::tests::expect_equal(one_remaining.kills, 1, "First enemy kill is credited");
            ioj::sim::tests::expect_equal(final.state,
                                          ioj::sim::MissionState::Succeeded,
                                          "Last enemy completes automatic kill target");
            ioj::sim::tests::expect_equal(final.kills, 2, "Both enemy kills are credited");
            break;
        }
        case Scenario::SuccessIsTerminal:
            ioj::sim::tests::expect_equal(final.state,
                                          ioj::sim::MissionState::Succeeded,
                                          "Mission remains successful after later destruction");
            ioj::sim::tests::expect_equal(final.fail_reason,
                                          ioj::sim::MissionFailReason::None,
                                          "Later destruction does not add a failure reason");
            ioj::sim::tests::expect_false(final.survivor_alive,
                                          "Defended entity is destroyed after success");
            break;
        case Scenario::ExplicitCompletionIsLatched:
            ioj::sim::tests::expect_true(first_completion_result,
                                         "Explicit completion performs the state transition");
            ioj::sim::tests::expect_false(duplicate_completion_result,
                                          "Duplicate completion is ignored");
            ioj::sim::tests::expect_equal(final.state,
                                          ioj::sim::MissionState::Succeeded,
                                          "Explicit completion leaves the mission succeeded");
            break;
        default:
            assert(false && "unreachable scenario");
            break;
    }
}

}
