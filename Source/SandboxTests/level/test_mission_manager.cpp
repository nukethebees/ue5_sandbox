#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include "test_mission_manager_scenario.h"

#include <SandboxCore/time_series_data.h>

#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

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
    auto const* capitals{simulation.get_capital_ships()};
    auto const hero{capitals->get_handle(hero_index)};
    auto const ordinary_enemy{ordinary_enemy_index == INDEX_NONE
                                  ? FRegistryEntityHandle{}
                                  : capitals->get_handle(ordinary_enemy_index)};
    auto const required_enemy{required_enemy_index == INDEX_NONE
                                  ? FRegistryEntityHandle{}
                                  : capitals->get_handle(required_enemy_index)};

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
                auto const second_enemy{simulation.get_capital_ships()->get_handle(1)};
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

FTestMissionManagerScenario::FTestMissionManagerScenario(FSimulationTestContext& context,
                                                         EScenario const new_scenario)
    : FSimulationTestScenario{context}
    , scenario{new_scenario} {}

void FTestMissionManagerScenario::configure_level(UWorld& world,
                                                  USpaceGameLevelConfig const& config,
                                                  FSoftTestAssertions& checks,
                                                  EScenario const scenario) {
    auto* const first_capital{ml::spawn_capital_proxy(
        world, config, checks, FName{TEXT("hero_capital")}, FVector{-2000.f, 0.f, 0.f})};
    if (!IsValid(first_capital)) {
        return;
    }
    switch (scenario) {
        case EScenario::SurviveTime: {
            break;
        }
        case EScenario::KillEnemies: {
            ml::spawn_capital_proxy(
                world, config, checks, FName{TEXT("enemy_capital")}, FVector{2000.f, 0.f, 0.f});
            break;
        }
        case EScenario::KillEnemiesWithinTime: {
            ml::spawn_capital_proxy(
                world, config, checks, FName{TEXT("enemy_capital")}, FVector{2000.f, 0.f, 0.f});
            break;
        }
        case EScenario::DefenceObjective: {
            break;
        }
        case EScenario::RequiredKillsObjective: {
            ml::spawn_capital_proxy(world,
                                    config,
                                    checks,
                                    FName{TEXT("ordinary_enemy_capital")},
                                    FVector{2000.f, 0.f, 0.f});
            ml::spawn_capital_proxy(world,
                                    config,
                                    checks,
                                    FName{TEXT("required_enemy_capital")},
                                    FVector{4000.f, 0.f, 0.f});
            break;
        }
        case EScenario::RequiredKillsTimeElapsed: {
            ml::spawn_capital_proxy(world,
                                    config,
                                    checks,
                                    FName{TEXT("required_enemy_capital")},
                                    FVector{2000.f, 0.f, 0.f});
            break;
        }
        case EScenario::AutomaticKillTarget: {
            first_capital->set_team(ETestTeam::Green);
            first_capital->set_initial_spawn_delay(60.f);
            auto* const first_enemy{ml::spawn_capital_proxy(world,
                                                            config,
                                                            checks,
                                                            FName{TEXT("first_enemy_capital")},
                                                            FVector{2000.f, 0.f, 0.f})};
            auto* const second_enemy{ml::spawn_capital_proxy(world,
                                                             config,
                                                             checks,
                                                             FName{TEXT("second_enemy_capital")},
                                                             FVector{4000.f, 0.f, 0.f})};
            if (IsValid(first_enemy)) {
                first_enemy->set_team(ETestTeam::Red);
                first_enemy->set_initial_spawn_delay(60.f);
            }
            if (IsValid(second_enemy)) {
                second_enemy->set_team(ETestTeam::Red);
                second_enemy->set_initial_spawn_delay(60.f);
            }
            break;
        }
        case EScenario::SuccessIsTerminal: {
            break;
        }
        case EScenario::ExplicitCompletionIsLatched: {
            break;
        }
        default: {
            checkNoEntry();
            break;
        }
    }
}

void FTestMissionManagerScenario::configure_mission_manager(UWorld& world,
                                                            ATestBatchOrchestrator& orchestrator,
                                                            EScenario const scenario) {
    auto const capitals{ml::get_actors<ATestCapitalShipProxy>(world)};
    auto const find_capital{[&capitals](FName const test_name) {
        auto* const* const capital{
            capitals.FindByPredicate([test_name](ATestCapitalShipProxy const* const candidate) {
                return candidate->get_test_name() == test_name;
            })};
        check(capital);
        return *capital;
    }};
    auto* const first_capital{find_capital(FName{TEXT("hero_capital")})};

    auto& manager{orchestrator.get_mission_definition()};
    manager.set_save_mission_results(false);

    switch (scenario) {
        case EScenario::SurviveTime: {
            manager.set_mission_mode(ETestMissionMode::SurviveTime);
            manager.set_target_time(short_mission_time);
            manager.add_entity_that_must_survive(*first_capital);
            break;
        }
        case EScenario::KillEnemies: {
            manager.set_mission_mode(ETestMissionMode::KillEnemies);
            manager.set_kill_target(1);
            manager.add_hero_entity(*first_capital);
            break;
        }
        case EScenario::KillEnemiesWithinTime: {
            manager.set_mission_mode(ETestMissionMode::KillEnemiesWithinTime);
            manager.set_target_time(short_mission_time);
            manager.set_kill_target(1);
            manager.add_hero_entity(*first_capital);
            break;
        }
        case EScenario::DefenceObjective: {
            manager.set_mission_mode(ETestMissionMode::SurviveTime);
            manager.set_target_time(long_mission_time);
            manager.add_entity_that_must_survive(*first_capital);
            break;
        }
        case EScenario::RequiredKillsObjective: {
            manager.set_mission_mode(ETestMissionMode::KillEnemies);
            manager.set_kill_target(1);
            manager.add_hero_entity(*first_capital);
            manager.add_entity_required_to_kill(
                *find_capital(FName{TEXT("required_enemy_capital")}));
            break;
        }
        case EScenario::RequiredKillsTimeElapsed: {
            manager.set_mission_mode(ETestMissionMode::SurviveTime);
            manager.set_target_time(short_mission_time);
            manager.add_entity_that_must_survive(*first_capital);
            manager.add_entity_required_to_kill(
                *find_capital(FName{TEXT("required_enemy_capital")}));
            break;
        }
        case EScenario::AutomaticKillTarget: {
            manager.set_mission_mode(ETestMissionMode::KillEnemies);
            manager.set_kill_target(0);
            manager.add_hero_entity(*first_capital);
            break;
        }
        case EScenario::SuccessIsTerminal: {
            manager.set_mission_mode(ETestMissionMode::SurviveTime);
            manager.set_target_time(short_mission_time);
            manager.add_entity_that_must_survive(*first_capital);
            break;
        }
        case EScenario::ExplicitCompletionIsLatched: {
            manager.set_mission_mode(ETestMissionMode::SurviveTime);
            manager.set_target_time(long_mission_time);
            manager.add_entity_that_must_survive(*first_capital);
            break;
        }
        default: {
            checkNoEntry();
            break;
        }
    }
}

void FTestMissionManagerScenario::setup_scenario(EScenario const new_scenario) {
    scenario = new_scenario;
    TestCommandBuilder.Do([this, new_scenario] {
        auto& world{context_.world};
        configure_level(world, context_.config, checks, new_scenario);
        auto* const orchestrator{&context_.orchestrator};
        if (checks.is_valid(orchestrator, TEXT("Orchestrator is available"))) {
            configure_mission_manager(world, *orchestrator, new_scenario);
        }
    });

    TestCommandBuilder.Do([this] { start_scenario(); })
        .Until(
            [this] {
                return scenario == EScenario::SuccessIsTerminal
                         ? test_driver->timeline.is_finished()
                         : mission_has_ended();
            },
            timeout)
        .Then([this] { check_scenario_result(); });
}

void FTestMissionManagerScenario::start_scenario() {
    initialise_test_driver();

    test_driver->orchestrator.start_simulation();
    auto* const manager{&test_driver->orchestrator.get_mission_manager()};

    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FTestMissionManagerScenario::on_end_tick));
    if (scenario == EScenario::KillEnemies) {
        test_driver->timeline.then_after(0.01, [this, manager] { queue_enemy_kill(*manager); });
    } else if (scenario == EScenario::DefenceObjective) {
        test_driver->timeline.then_after(0.01,
                                         [this, manager] { queue_defended_entity_kill(*manager); });
    } else if (scenario == EScenario::RequiredKillsObjective) {
        test_driver->timeline.then_after(0.01, [this, manager] { queue_enemy_kill(*manager); })
            .then_after(0.19, [this, manager] { queue_required_entity_kill(*manager); });
    } else if (scenario == EScenario::AutomaticKillTarget) {
        test_driver->timeline.then_after(0.01, [this, manager] { queue_enemy_kill(*manager); })
            .then_after(0.19, [this, manager] { queue_enemy_kill(*manager); });
    } else if (scenario == EScenario::SuccessIsTerminal) {
        test_driver->timeline.at(0.15, [this, manager] { queue_defended_entity_kill(*manager); })
            .finish_at(0.25);
    } else if (scenario == EScenario::ExplicitCompletionIsLatched) {
        test_driver->timeline.then_after(0.01, [this, manager] {
            first_completion_result_ = manager->complete_mission();
            duplicate_completion_result_ = manager->complete_mission();
        });
    }
    TestRunner->TestEqual(
        TEXT("Mission starts running"), manager->get_mission_state(), ETestMissionState::Running);
    TestRunner->TestFalse(TEXT("Mission result saving is disabled"),
                          manager->should_save_mission_results());

    auto const surviving_health{manager->get_entity_health_that_must_survive()};
    TestRunner->TestEqual(TEXT("Survival health data matches objective handles"),
                          surviving_health.Num(),
                          manager->get_entity_handles_that_must_survive().Num());
    TestRunner->TestEqual(TEXT("Survival IDs match objective handles"),
                          manager->get_entity_ids_that_must_survive().Num(),
                          manager->get_entity_handles_that_must_survive().Num());
    TestRunner->TestEqual(TEXT("Survival types match objective handles"),
                          manager->get_entity_types_that_must_survive().Num(),
                          manager->get_entity_handles_that_must_survive().Num());
    if (!surviving_health.IsEmpty()) {
        TestRunner->TestTrue(TEXT("Survival health captures a positive maximum"),
                             surviving_health[0].max_health > 0);
        TestRunner->TestEqual(TEXT("Initial survival health is full"),
                              surviving_health[0].health,
                              surviving_health[0].max_health);
    }

    auto const required_kill_health{manager->get_entity_health_required_to_kill()};
    TestRunner->TestEqual(TEXT("Required-kill health data matches objective handles"),
                          required_kill_health.Num(),
                          manager->get_entity_handles_required_to_kill().Num());
    TestRunner->TestEqual(TEXT("Required-kill IDs match objective handles"),
                          manager->get_entity_ids_required_to_kill().Num(),
                          manager->get_entity_handles_required_to_kill().Num());
    TestRunner->TestEqual(TEXT("Required-kill types match objective handles"),
                          manager->get_entity_types_required_to_kill().Num(),
                          manager->get_entity_handles_required_to_kill().Num());
    if (!required_kill_health.IsEmpty()) {
        TestRunner->TestTrue(TEXT("Required-kill health captures a positive maximum"),
                             required_kill_health[0].max_health > 0);
        TestRunner->TestEqual(TEXT("Initial required-kill health is full"),
                              required_kill_health[0].health,
                              required_kill_health[0].max_health);
    }
}

void FTestMissionManagerScenario::on_end_tick(ATestBatchOrchestrator&) {
    auto const& manager{test_driver->orchestrator.get_mission_manager()};

    FSimulationSample sample{};
    sample.mission_state = manager.get_mission_state();
    sample.mission_fail_reason = manager.get_mission_fail_reason();
    sample.mission_kills = manager.get_mission_kills();
    sample.kill_target = manager.get_kill_target();
    auto const surviving_handles{manager.get_entity_handles_that_must_survive()};
    sample.surviving_entity_alive =
        !surviving_handles.IsEmpty() &&
        test_driver->get_registry().is_valid_alive(surviving_handles[0]);
    for (auto const& health : manager.get_entity_health_that_must_survive()) {
        sample.surviving_entity_health.Add(health.health);
    }
    for (auto const& health : manager.get_entity_health_required_to_kill()) {
        sample.required_kill_entity_health.Add(health.health);
    }

    samples.add(test_driver->get_time(), MoveTemp(sample));
    test_driver->advance_timeline();
}

void FTestMissionManagerScenario::queue_enemy_kill(FTestMissionManager const& manager) {
    auto const hero_handles{manager.get_hero_entity_handles()};
    check(hero_handles.Num() == 1);

    auto const hero{hero_handles[0]};
    auto const required_handles{manager.get_entity_handles_required_to_kill()};
    auto const& capitals{test_driver->get_capital_ships()};
    auto const n{capitals.get_num_instances()};

    FRegistryEntityHandle enemy;
    for (int32 i{0}; i < n; ++i) {
        auto const handle{capitals.get_handle(i)};
        if (handle != hero && !required_handles.Contains(handle)) {
            enemy = handle;
            break;
        }
    }
    check(enemy.is_valid());

    TArray<FRegistryEntityHandle> const targets{enemy};
    test_driver->queue_kills(targets, hero);
}

void FTestMissionManagerScenario::queue_defended_entity_kill(FTestMissionManager const& manager) {
    auto const defended_handles{manager.get_entity_handles_that_must_survive()};
    check(defended_handles.Num() == 1);

    TArray<FRegistryEntityHandle> const targets{defended_handles[0]};
    auto const damage{test_driver->get_capital_ships().get_health(defended_handles[0])};
    test_driver->queue_damage(targets, damage);
}

void FTestMissionManagerScenario::queue_required_entity_kill(FTestMissionManager const& manager) {
    auto const required_handles{manager.get_entity_handles_required_to_kill()};
    check(required_handles.Num() == 1);

    TArray<FRegistryEntityHandle> const targets{required_handles[0]};
    auto const damage{test_driver->get_capital_ships().get_health(required_handles[0])};
    test_driver->queue_damage(targets, damage);
}

auto FTestMissionManagerScenario::mission_has_ended() const -> bool {
    auto const& manager{test_driver->orchestrator.get_mission_manager()};
    auto const state{manager.get_mission_state()};
    return state == ETestMissionState::Succeeded || state == ETestMissionState::Failed;
}

void FTestMissionManagerScenario::check_scenario_result() {
    ml::check_samples_recorded(samples.num(), checks, TEXT("Mission simulation samples recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{samples.last_value()};

    switch (scenario) {
        case EScenario::SurviveTime: {
            TestRunner->TestEqual(TEXT("Survive-time mission succeeds"),
                                  sample.mission_state,
                                  ETestMissionState::Succeeded);
            TestRunner->TestEqual(TEXT("Successful mission has no failure reason"),
                                  sample.mission_fail_reason,
                                  ETestMissionFailReason::None);
            break;
        }
        case EScenario::KillEnemies: {
            TestRunner->TestEqual(
                TEXT("Kill mission succeeds"), sample.mission_state, ETestMissionState::Succeeded);
            TestRunner->TestEqual(
                TEXT("Hero kill contributes to mission"), sample.mission_kills, 1);
            break;
        }
        case EScenario::KillEnemiesWithinTime: {
            TestRunner->TestEqual(
                TEXT("Timed kill mission fails"), sample.mission_state, ETestMissionState::Failed);
            TestRunner->TestEqual(TEXT("Timed mission reports elapsed time"),
                                  sample.mission_fail_reason,
                                  ETestMissionFailReason::TimeElapsed);
            break;
        }
        case EScenario::DefenceObjective: {
            TestRunner->TestEqual(TEXT("Defence objective failure fails mission"),
                                  sample.mission_state,
                                  ETestMissionState::Failed);
            TestRunner->TestEqual(TEXT("Defence failure reason is retained"),
                                  sample.mission_fail_reason,
                                  ETestMissionFailReason::DefenceObjectiveFailed);
            TestRunner->TestTrue(TEXT("Defence health is sampled"),
                                 !sample.surviving_entity_health.IsEmpty());
            if (!sample.surviving_entity_health.IsEmpty()) {
                TestRunner->TestEqual(TEXT("Destroyed defence objective reports zero health"),
                                      sample.surviving_entity_health[0],
                                      0);
            }
            break;
        }
        case EScenario::RequiredKillsObjective: {
            auto const& gated_sample{samples.nearest_value(0.1)};
            TestRunner->TestEqual(TEXT("Normal kill target does not bypass required kill"),
                                  gated_sample.mission_state,
                                  ETestMissionState::Running);
            TestRunner->TestEqual(TEXT("Normal kill target is met before required kill"),
                                  gated_sample.mission_kills,
                                  1);
            TestRunner->TestTrue(TEXT("Required target remains healthy while mission is gated"),
                                 !gated_sample.required_kill_entity_health.IsEmpty() &&
                                     gated_sample.required_kill_entity_health[0] > 0);
            TestRunner->TestEqual(TEXT("Required-kill mission succeeds"),
                                  sample.mission_state,
                                  ETestMissionState::Succeeded);
            TestRunner->TestEqual(
                TEXT("Uncredited required kill preserves mission kills"), sample.mission_kills, 1);
            TestRunner->TestTrue(TEXT("Required-kill health is sampled"),
                                 !sample.required_kill_entity_health.IsEmpty());
            if (!sample.required_kill_entity_health.IsEmpty()) {
                TestRunner->TestEqual(TEXT("Destroyed required target reports zero health"),
                                      sample.required_kill_entity_health[0],
                                      0);
            }
            break;
        }
        case EScenario::RequiredKillsTimeElapsed: {
            TestRunner->TestEqual(TEXT("Incomplete required kill fails survive-time mission"),
                                  sample.mission_state,
                                  ETestMissionState::Failed);
            TestRunner->TestEqual(TEXT("Incomplete required kill reports elapsed time"),
                                  sample.mission_fail_reason,
                                  ETestMissionFailReason::TimeElapsed);
            TestRunner->TestTrue(TEXT("Required target remains alive at timeout"),
                                 !sample.required_kill_entity_health.IsEmpty() &&
                                     sample.required_kill_entity_health[0] > 0);
            break;
        }
        case EScenario::AutomaticKillTarget: {
            auto const& one_remaining_sample{samples.nearest_value(0.1)};
            TestRunner->TestEqual(TEXT("Automatic target counts both initial enemies"),
                                  one_remaining_sample.kill_target,
                                  2);
            TestRunner->TestEqual(TEXT("Mission remains running with one enemy left"),
                                  one_remaining_sample.mission_state,
                                  ETestMissionState::Running);
            TestRunner->TestEqual(
                TEXT("First enemy kill is credited"), one_remaining_sample.mission_kills, 1);
            TestRunner->TestEqual(TEXT("Last enemy completes automatic kill target"),
                                  sample.mission_state,
                                  ETestMissionState::Succeeded);
            TestRunner->TestEqual(TEXT("Both enemy kills are credited"), sample.mission_kills, 2);
            break;
        }
        case EScenario::SuccessIsTerminal: {
            auto succeeded_before_destruction{false};
            auto const n_samples{samples.num()};
            for (int32 i{0}; i < n_samples; ++i) {
                if (samples.time_at(i) < 0.15 &&
                    samples.value_at(i).mission_state == ETestMissionState::Succeeded) {
                    succeeded_before_destruction = true;
                }
            }
            TestRunner->TestTrue(TEXT("Mission succeeds before defended entity is destroyed"),
                                 succeeded_before_destruction);
            TestRunner->TestEqual(TEXT("Mission remains successful after later destruction"),
                                  sample.mission_state,
                                  ETestMissionState::Succeeded);
            TestRunner->TestEqual(TEXT("Later destruction does not add a failure reason"),
                                  sample.mission_fail_reason,
                                  ETestMissionFailReason::None);
            TestRunner->TestFalse(TEXT("Defended entity is destroyed after success"),
                                  sample.surviving_entity_alive);
            break;
        }
        case EScenario::ExplicitCompletionIsLatched: {
            TestRunner->TestTrue(TEXT("Explicit completion performs the state transition"),
                                 first_completion_result_);
            TestRunner->TestFalse(TEXT("Duplicate completion is ignored"),
                                  duplicate_completion_result_);
            TestRunner->TestEqual(TEXT("Explicit completion leaves the mission succeeded"),
                                  sample.mission_state,
                                  ETestMissionState::Succeeded);
            break;
        }
        default: {
            checkNoEntry();
            break;
        }
    }
}

void FTestMissionManagerScenario::run() {
    setup_scenario(scenario);
}
}
