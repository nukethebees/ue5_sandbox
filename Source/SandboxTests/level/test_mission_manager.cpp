#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include "test_mission_manager.h"

#include <SandboxCore/time_series_data.h>

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/missions/TestMissionManager.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Engine/World.h>
#include <Misc/Optional.h>

namespace ml {
namespace {
auto add_worldless_capital(FLevelSimulationInitData& data,
                           FVector const location,
                           ETestTeam const team = ETestTeam::White) -> int32 {
    auto const index{data.capital_spawns.num()};
    data.capital_spawns.add_defaulted(1);
    ml::assign(data.capital_spawns.locations, index, location);
    data.capital_spawns.teams[index] = team;
    data.capital_spawns.healths[index] = data.capital_ships.max_health;
    data.capital_spawns.initial_spawn_delays[index] = 60.f;
    data.capital_spawns.spawn_cooldowns[index] = 60.f;
    return index;
}
}

void run_worldless_mission_manager_scenario(FAutomationTestBase& test,
                                            USpaceGameLevelConfig const& config,
                                            EMissionManagerScenario const scenario) {
    using EScenario = EMissionManagerScenario;

    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    auto const hero_index{add_worldless_capital(
        data,
        FVector{-2000.f, 0.f, 0.f},
        scenario == EScenario::AutomaticKillTarget ? ETestTeam::Green : ETestTeam::White)};
    int32 ordinary_enemy_index{INDEX_NONE};
    int32 required_enemy_index{INDEX_NONE};
    if (scenario == EScenario::KillEnemies || scenario == EScenario::KillEnemiesWithinTime) {
        ordinary_enemy_index = add_worldless_capital(data, FVector{2000.f, 0.f, 0.f});
    } else if (scenario == EScenario::RequiredKillsObjective) {
        ordinary_enemy_index = add_worldless_capital(data, FVector{2000.f, 0.f, 0.f});
        required_enemy_index = add_worldless_capital(data, FVector{4000.f, 0.f, 0.f});
    } else if (scenario == EScenario::RequiredKillsTimeElapsed) {
        required_enemy_index = add_worldless_capital(data, FVector{2000.f, 0.f, 0.f});
    } else if (scenario == EScenario::AutomaticKillTarget) {
        ordinary_enemy_index =
            add_worldless_capital(data, FVector{2000.f, 0.f, 0.f}, ETestTeam::Red);
        add_worldless_capital(data, FVector{4000.f, 0.f, 0.f}, ETestTeam::Red);
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    auto& simulation{harness.get_simulation()};
    auto& manager{simulation.get_mission_manager()};
    auto const& capitals{simulation.get_capital_ships()};
    auto const hero{capitals.get_handle(hero_index)};
    auto const ordinary_enemy{ordinary_enemy_index == INDEX_NONE
                                  ? FRegistryEntityHandle{}
                                  : capitals.get_handle(ordinary_enemy_index)};
    auto const required_enemy{required_enemy_index == INDEX_NONE
                                  ? FRegistryEntityHandle{}
                                  : capitals.get_handle(required_enemy_index)};

    manager.set_save_mission_results(false);
    switch (scenario) {
        case EScenario::SurviveTime:
            manager.set_mission_mode(ETestMissionMode::SurviveTime);
            manager.set_target_time(0.1f);
            manager.add_entity_that_must_survive(hero);
            break;
        case EScenario::KillEnemies:
            manager.set_mission_mode(ETestMissionMode::KillEnemies);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            break;
        case EScenario::KillEnemiesWithinTime:
            manager.set_mission_mode(ETestMissionMode::KillEnemiesWithinTime);
            manager.set_target_time(0.1f);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            break;
        case EScenario::DefenceObjective:
        case EScenario::SuccessIsTerminal:
        case EScenario::ExplicitCompletionIsLatched:
            manager.set_mission_mode(ETestMissionMode::SurviveTime);
            manager.set_target_time(scenario == EScenario::SuccessIsTerminal ? 0.1f : 10.f);
            manager.add_entity_that_must_survive(hero);
            break;
        case EScenario::RequiredKillsObjective:
            manager.set_mission_mode(ETestMissionMode::KillEnemies);
            manager.set_kill_target(1);
            manager.add_hero_entity(hero);
            manager.add_entity_required_to_kill(required_enemy);
            break;
        case EScenario::RequiredKillsTimeElapsed:
            manager.set_mission_mode(ETestMissionMode::SurviveTime);
            manager.set_target_time(0.1f);
            manager.add_entity_that_must_survive(hero);
            manager.add_entity_required_to_kill(required_enemy);
            break;
        case EScenario::AutomaticKillTarget:
            manager.set_mission_mode(ETestMissionMode::KillEnemies);
            manager.set_kill_target(0);
            manager.add_hero_entity(hero);
            break;
        default:
            checkNoEntry();
            break;
    }
    harness.finish_initialisation();

    struct Sample {
        ETestMissionState state{ETestMissionState::NotStarted};
        ETestMissionFailReason fail_reason{ETestMissionFailReason::None};
        int32 kills{};
        int32 kill_target{};
        bool survivor_alive{};
        int32 survivor_health{};
        int32 required_health{};
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
            !survivors.IsEmpty() && harness.get_registry().is_valid_alive(survivors[0]);
        auto const survivor_health{manager.get_entity_health_that_must_survive()};
        sample.survivor_health = survivor_health.IsEmpty() ? 0 : survivor_health[0].health;
        auto const required_health{manager.get_entity_health_required_to_kill()};
        sample.required_health = required_health.IsEmpty() ? 0 : required_health[0].health;
        samples.add(harness.get_time(), sample);
    };

    auto first_completion_result{false};
    auto duplicate_completion_result{true};
    if (scenario == EScenario::KillEnemies) {
        harness.timeline.then_after(0.01,
                                    [&] { harness.queue_kills(TArray{ordinary_enemy}, hero); });
    } else if (scenario == EScenario::DefenceObjective) {
        harness.timeline.then_after(0.01, [&] { harness.queue_kills(TArray{hero}); });
    } else if (scenario == EScenario::RequiredKillsObjective) {
        harness.timeline
            .then_after(0.01, [&] { harness.queue_kills(TArray{ordinary_enemy}, hero); })
            .then_after(0.19, [&] { harness.queue_kills(TArray{required_enemy}); });
    } else if (scenario == EScenario::AutomaticKillTarget) {
        harness.timeline
            .then_after(0.01, [&] { harness.queue_kills(TArray{ordinary_enemy}, hero); })
            .then_after(0.19, [&] {
                auto const second_enemy{simulation.get_capital_ships().get_handle(1)};
                harness.queue_kills(TArray{second_enemy}, hero);
            });
    } else if (scenario == EScenario::SuccessIsTerminal) {
        harness.timeline.at(0.15, [&] { harness.queue_kills(TArray{hero}); });
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

    test.TestEqual(
        TEXT("Mission starts running"), manager.get_mission_state(), ETestMissionState::Running);
    test.TestFalse(TEXT("Mission result saving is disabled"),
                   manager.should_save_mission_results());
    test.TestTrue(TEXT("Mission timeline completes within its simulation-time limit"),
                  harness.run_until_timeline_finished(1.0));
    test.TestTrue(TEXT("Mission simulation samples recorded"), !samples.is_empty());
    if (samples.is_empty()) {
        return;
    }

    auto const& final{samples.last_value()};
    switch (scenario) {
        case EScenario::SurviveTime:
            test.TestEqual(
                TEXT("Survive-time mission succeeds"), final.state, ETestMissionState::Succeeded);
            test.TestEqual(TEXT("Successful mission has no failure reason"),
                           final.fail_reason,
                           ETestMissionFailReason::None);
            break;
        case EScenario::KillEnemies:
            test.TestEqual(
                TEXT("Kill mission succeeds"), final.state, ETestMissionState::Succeeded);
            test.TestEqual(TEXT("Hero kill contributes to mission"), final.kills, 1);
            break;
        case EScenario::KillEnemiesWithinTime:
            test.TestEqual(
                TEXT("Timed kill mission fails"), final.state, ETestMissionState::Failed);
            test.TestEqual(TEXT("Timed mission reports elapsed time"),
                           final.fail_reason,
                           ETestMissionFailReason::TimeElapsed);
            break;
        case EScenario::DefenceObjective:
            test.TestEqual(TEXT("Defence objective failure fails mission"),
                           final.state,
                           ETestMissionState::Failed);
            test.TestEqual(TEXT("Defence failure reason is retained"),
                           final.fail_reason,
                           ETestMissionFailReason::DefenceObjectiveFailed);
            test.TestEqual(
                TEXT("Destroyed defence objective reports zero health"), final.survivor_health, 0);
            break;
        case EScenario::RequiredKillsObjective: {
            auto const& gated{samples.nearest_value(0.1)};
            test.TestEqual(TEXT("Normal kill target does not bypass required kill"),
                           gated.state,
                           ETestMissionState::Running);
            test.TestEqual(TEXT("Normal kill target is met before required kill"), gated.kills, 1);
            test.TestTrue(TEXT("Required target remains healthy while mission is gated"),
                          gated.required_health > 0);
            test.TestEqual(
                TEXT("Required-kill mission succeeds"), final.state, ETestMissionState::Succeeded);
            test.TestEqual(
                TEXT("Uncredited required kill preserves mission kills"), final.kills, 1);
            test.TestEqual(
                TEXT("Destroyed required target reports zero health"), final.required_health, 0);
            break;
        }
        case EScenario::RequiredKillsTimeElapsed:
            test.TestEqual(TEXT("Incomplete required kill fails survive-time mission"),
                           final.state,
                           ETestMissionState::Failed);
            test.TestEqual(TEXT("Incomplete required kill reports elapsed time"),
                           final.fail_reason,
                           ETestMissionFailReason::TimeElapsed);
            test.TestTrue(TEXT("Required target remains alive at timeout"),
                          final.required_health > 0);
            break;
        case EScenario::AutomaticKillTarget: {
            auto const& one_remaining{samples.nearest_value(0.1)};
            test.TestEqual(
                TEXT("Automatic target counts both initial enemies"), one_remaining.kill_target, 2);
            test.TestEqual(TEXT("Mission remains running with one enemy left"),
                           one_remaining.state,
                           ETestMissionState::Running);
            test.TestEqual(TEXT("First enemy kill is credited"), one_remaining.kills, 1);
            test.TestEqual(TEXT("Last enemy completes automatic kill target"),
                           final.state,
                           ETestMissionState::Succeeded);
            test.TestEqual(TEXT("Both enemy kills are credited"), final.kills, 2);
            break;
        }
        case EScenario::SuccessIsTerminal:
            test.TestEqual(TEXT("Mission remains successful after later destruction"),
                           final.state,
                           ETestMissionState::Succeeded);
            test.TestEqual(TEXT("Later destruction does not add a failure reason"),
                           final.fail_reason,
                           ETestMissionFailReason::None);
            test.TestFalse(TEXT("Defended entity is destroyed after success"),
                           final.survivor_alive);
            break;
        case EScenario::ExplicitCompletionIsLatched:
            test.TestTrue(TEXT("Explicit completion performs the state transition"),
                          first_completion_result);
            test.TestFalse(TEXT("Duplicate completion is ignored"), duplicate_completion_result);
            test.TestEqual(TEXT("Explicit completion leaves the mission succeeded"),
                           final.state,
                           ETestMissionState::Succeeded);
            break;
        default:
            checkNoEntry();
            break;
    }
}

}
