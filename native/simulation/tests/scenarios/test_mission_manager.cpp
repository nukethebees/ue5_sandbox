#include <ioj/sim/testing/level_sim_test_access.h>
#include "../support/simulation_test_support.h"

#include "test_mission_manager.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/mission_manager.h>

namespace ioj::sim {
namespace {
auto add_worldless_capital(LevelSimInitData& data,
                           ml::Vector3d const location,
                           Team const team = Team::White) -> std::int32_t {
    auto const row{data.level_events.initial_spawns.capital_spawns.num()};
    tests::add_capital_spawn(data, to_float(location), team, -1, 60.f, 60.f);
    return row;
}
}

void run_worldless_mission_manager_scenario(tests::SimulationFixture const& config,
                                            MissionManagerScenario const scenario) {
    using Scenario = MissionManagerScenario;

    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const hero_index{add_worldless_capital(
        data,
        ml::Vector3d{-2000.f, 0.f, 0.f},
        scenario == Scenario::AutomaticKillTarget ? Team::Green : Team::White)};
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
            add_worldless_capital(data, ml::Vector3d{2000.f, 0.f, 0.f}, Team::Red);
        add_worldless_capital(data, ml::Vector3d{4000.f, 0.f, 0.f}, Team::Red);
    }

    auto& mission{data.level_events.initialisation.mission.emplace()};
    mission.save_results = false;
    auto const& entity_indices{data.level_events.initial_spawns.capital_spawns.entity_indices};
    auto add_hero{[&] { mission.hero_entity_indices.push_back(entity_indices[hero_index]); }};
    auto add_survivor{
        [&] { mission.must_survive_entity_indices.push_back(entity_indices[hero_index]); }};
    auto add_required_enemy{[&] {
        mission.required_kill_entity_indices.push_back(entity_indices[required_enemy_index]);
    }};
    switch (scenario) {
        case Scenario::SurviveTime:
            mission.mode = levels::LevelMissionMode::SurviveTime;
            mission.time_limit_seconds = 0.1f;
            add_survivor();
            break;
        case Scenario::KillEnemies:
            mission.mode = levels::LevelMissionMode::KillEnemies;
            mission.kill_count = 1;
            add_hero();
            break;
        case Scenario::KillEnemiesWithinTime:
            mission.mode = levels::LevelMissionMode::KillEnemiesWithinTime;
            mission.time_limit_seconds = 0.1f;
            mission.kill_count = 1;
            add_hero();
            break;
        case Scenario::DefenceObjective:
        case Scenario::SuccessIsTerminal:
        case Scenario::ExplicitCompletionIsLatched:
            mission.mode = levels::LevelMissionMode::SurviveTime;
            mission.time_limit_seconds = scenario == Scenario::SuccessIsTerminal ? 0.1f : 10.f;
            add_survivor();
            break;
        case Scenario::RequiredKillsObjective:
            mission.mode = levels::LevelMissionMode::KillEnemies;
            mission.kill_count = 1;
            add_hero();
            add_required_enemy();
            break;
        case Scenario::RequiredKillsTimeElapsed:
            mission.mode = levels::LevelMissionMode::SurviveTime;
            mission.time_limit_seconds = 0.1f;
            add_survivor();
            add_required_enemy();
            break;
        case Scenario::AutomaticKillTarget:
            mission.mode = levels::LevelMissionMode::KillEnemies;
            mission.kill_count = 0;
            add_hero();
            break;
        default:
            assert(false && "unreachable scenario");
            break;
    }

    tests::WorldlessSimulationTest harness{std::move(data)};
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

    harness.finish_initialisation();

    struct Sample {
        MissionState state{MissionState::NotStarted};
        MissionFailReason fail_reason{MissionFailReason::None};
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
            first_completion_result = LevelSimTestAccess::complete_mission(simulation);
            duplicate_completion_result = LevelSimTestAccess::complete_mission(simulation);
        });
    }
    auto const end_time{scenario == Scenario::RequiredKillsObjective ||
                                scenario == Scenario::AutomaticKillTarget
                            ? 0.3
                            : 0.25};
    harness.timeline.finish_at(end_time);

    tests::expect_equal(
        manager.get_mission_state(), MissionState::Running, "Mission starts running");
    tests::expect_false(manager.should_save_mission_results(), "Mission result saving is disabled");
    tests::expect_true(harness.run_until_timeline_finished(1.0),
                       "Mission timeline completes within its simulation-time limit");
    tests::expect_true(!samples.is_empty(), "Mission simulation samples recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& final{samples.last_value()};
    switch (scenario) {
        case Scenario::SurviveTime:
            tests::expect_equal(
                final.state, MissionState::Succeeded, "Survive-time mission succeeds");
            tests::expect_equal(final.fail_reason,
                                MissionFailReason::None,
                                "Successful mission has no failure reason");
            break;
        case Scenario::KillEnemies:
            tests::expect_equal(final.state, MissionState::Succeeded, "Kill mission succeeds");
            tests::expect_equal(final.kills, 1, "Hero kill contributes to mission");
            break;
        case Scenario::KillEnemiesWithinTime:
            tests::expect_equal(final.state, MissionState::Failed, "Timed kill mission fails");
            tests::expect_equal(final.fail_reason,
                                MissionFailReason::TimeElapsed,
                                "Timed mission reports elapsed time");
            break;
        case Scenario::DefenceObjective:
            tests::expect_equal(
                final.state, MissionState::Failed, "Defence objective failure fails mission");
            tests::expect_equal(final.fail_reason,
                                MissionFailReason::DefenceObjectiveFailed,
                                "Defence failure reason is retained");
            tests::expect_equal(
                final.survivor_health, 0, "Destroyed defence objective reports zero health");
            break;
        case Scenario::RequiredKillsObjective: {
            auto const& gated{samples.nearest_value(0.1)};
            tests::expect_equal(gated.state,
                                MissionState::Running,
                                "Normal kill target does not bypass required kill");
            tests::expect_equal(gated.kills, 1, "Normal kill target is met before required kill");
            tests::expect_true(gated.required_health > 0,
                               "Required target remains healthy while mission is gated");
            tests::expect_equal(
                final.state, MissionState::Succeeded, "Required-kill mission succeeds");
            tests::expect_equal(final.kills, 1, "Uncredited required kill preserves mission kills");
            tests::expect_equal(
                final.required_health, 0, "Destroyed required target reports zero health");
            break;
        }
        case Scenario::RequiredKillsTimeElapsed:
            tests::expect_equal(final.state,
                                MissionState::Failed,
                                "Incomplete required kill fails survive-time mission");
            tests::expect_equal(final.fail_reason,
                                MissionFailReason::TimeElapsed,
                                "Incomplete required kill reports elapsed time");
            tests::expect_true(final.required_health > 0,
                               "Required target remains alive at timeout");
            break;
        case Scenario::AutomaticKillTarget: {
            auto const& one_remaining{samples.nearest_value(0.1)};
            tests::expect_equal(
                one_remaining.kill_target, 2, "Automatic target counts both initial enemies");
            tests::expect_equal(one_remaining.state,
                                MissionState::Running,
                                "Mission remains running with one enemy left");
            tests::expect_equal(one_remaining.kills, 1, "First enemy kill is credited");
            tests::expect_equal(
                final.state, MissionState::Succeeded, "Last enemy completes automatic kill target");
            tests::expect_equal(final.kills, 2, "Both enemy kills are credited");
            break;
        }
        case Scenario::SuccessIsTerminal:
            tests::expect_equal(final.state,
                                MissionState::Succeeded,
                                "Mission remains successful after later destruction");
            tests::expect_equal(final.fail_reason,
                                MissionFailReason::None,
                                "Later destruction does not add a failure reason");
            tests::expect_false(final.survivor_alive, "Defended entity is destroyed after success");
            break;
        case Scenario::ExplicitCompletionIsLatched:
            tests::expect_true(first_completion_result,
                               "Explicit completion performs the state transition");
            tests::expect_false(duplicate_completion_result, "Duplicate completion is ignored");
            tests::expect_equal(final.state,
                                MissionState::Succeeded,
                                "Explicit completion leaves the mission succeeded");
            break;
        default:
            assert(false && "unreachable scenario");
            break;
    }
}

}
