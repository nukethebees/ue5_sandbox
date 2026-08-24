#include <SandboxTests/support/test_setup.h>
#include "test_batch_orchestrator_reset_scenario.h"

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestSimulationDriver.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/combat/lasers/TestLasers.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/defences/turrets/TestStaticTurrets.h>
#include <SpaceGame/defences/spinners/TestTubeSpinners.h>
#include <SpaceGame/effects/DelayedNiagaraSpawner.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Engine/LevelScriptActor.h>
#include <EngineUtils.h>
#include <GameFramework/Actor.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/GameStateBase.h>
#include <GameFramework/HUD.h>
#include <GameFramework/PhysicsVolume.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/PlayerState.h>
#include <GameFramework/WorldSettings.h>
#include <Misc/Optional.h>

namespace ml {
FTestBatchOrchestratorResetScenario::FTestBatchOrchestratorResetScenario(
    FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    test_driver.Reset();
    initial_samples.reset();
    reset_samples.reset();
    for (auto& blocker : blockers) {
        blocker.Reset();
    }
    for (auto& actor : old_owned_actors) {
        actor = nullptr;
    }
    for (auto& actor : old_transient_actors) {
        actor = nullptr;
    }
    old_transient_actor_count = 0;
    initial_actor_count = 0;
    reset_complete = false;

    TestCommandBuilder.Do([this] { spawn_blockers(context_.world); });
}

void FTestBatchOrchestratorResetScenario::tear_down() {
    if (test_driver.IsSet()) {
        test_driver->orchestrator.clear_end_tick_test_hook();
        test_driver->orchestrator.pause_simulation();
    }
}

void FTestBatchOrchestratorResetScenario::spawn_blockers(UWorld& world) {
    for (int32 i{0}; i < blocker_count; ++i) {
        auto* const blocker{world.SpawnActor<AActor>(
            AActor::StaticClass(), FVector{1000.f * i, 0.f, 0.f}, FRotator::ZeroRotator)};
        if (!checks.is_true(IsValid(blocker), TEXT("Reset blocker is spawned"), i)) {
            return;
        }

        blockers[i] = blocker;
    }
}

auto FTestBatchOrchestratorResetScenario::count_actors(UWorld const& world) const -> int32 {
    int32 count{0};
    for (TActorIterator<AActor> it{&world}; it; ++it) {
        ++count;
    }
    return count;
}

auto FTestBatchOrchestratorResetScenario::is_core_runtime_actor(AActor const& actor) const -> bool {
    return ml::actor_is_any<AWorldSettings,
                            AGameModeBase,
                            AGameStateBase,
                            APlayerController,
                            APlayerState,
                            AHUD,
                            ALevelScriptActor,
                            APhysicsVolume>(actor);
}

void FTestBatchOrchestratorResetScenario::save_old_owned_actors(
    ATestBatchOrchestrator const& orchestrator) {
    old_owned_actors[0] = const_cast<ATestSpaceShip*>(orchestrator.get_player_ship());
    old_owned_actors[1] = const_cast<ATestLasers*>(orchestrator.get_lasers());
    old_owned_actors[2] = const_cast<ATestCapitalShips*>(orchestrator.get_capital_ships());
    old_owned_actors[3] =
        const_cast<ATestCapitalShipFighters*>(orchestrator.get_capital_ship_fighters());
    old_owned_actors[4] = const_cast<ATestStaticTurrets*>(orchestrator.get_turrets());
    old_owned_actors[5] = const_cast<ATestTubeSpinners*>(orchestrator.get_spinners());
    old_owned_actors[6] = const_cast<ADelayedNiagaraSpawner*>(orchestrator.get_niagara_spawner());
}

void FTestBatchOrchestratorResetScenario::save_old_transient_actors(
    ATestBatchOrchestrator const& orchestrator) {
    for (TActorIterator<AActor> it{test_driver->get_world()}; it; ++it) {
        auto* const actor{*it};
        if (actor == &orchestrator || is_core_runtime_actor(*actor)) {
            continue;
        }

        check(old_transient_actor_count < old_transient_actors.Num());
        old_transient_actors[old_transient_actor_count] = actor;
        ++old_transient_actor_count;
    }
}

void FTestBatchOrchestratorResetScenario::sample(ATestBatchOrchestrator& orchestrator) {
    FSimulationSample const sample{
        .actor_count = count_actors(*test_driver->get_world()),
    };

    if (reset_complete) {
        reset_samples.add(orchestrator.get_simulation_time(), sample);
    } else {
        initial_samples.add(orchestrator.get_simulation_time(), sample);
    }
}

void FTestBatchOrchestratorResetScenario::on_end_tick(ATestBatchOrchestrator& orchestrator) {
    sample(orchestrator);
}

void FTestBatchOrchestratorResetScenario::start_initial_simulation() {
    test_driver = ml::TestSimulationDriver::from_world(context_.world);
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    ml::reset_and_reserve_time_series(
        test_driver->orchestrator, reset_time, initial_samples, reset_samples);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTestBatchOrchestratorResetScenario::on_end_tick));
    test_driver->orchestrator.start_simulation();
}

void FTestBatchOrchestratorResetScenario::reset_simulation() {
    check(!initial_samples.is_empty());

    initial_actor_count = initial_samples.last_value().actor_count;
    save_old_owned_actors(test_driver->orchestrator);
    save_old_transient_actors(test_driver->orchestrator);
    auto const& telemetry{
        test_driver->orchestrator.get_level_telemetry_manager().get_active_entity_count_data()};
    checks.is_true(!telemetry.is_empty(), TEXT("Initial level telemetry is populated"));
    test_driver->orchestrator.reset_for_new_level();
    checks.is_true(telemetry.is_empty(), TEXT("Level reset clears telemetry"));
    reset_complete = true;
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTestBatchOrchestratorResetScenario::on_end_tick));
    test_driver->orchestrator.start_simulation();
    checks.are_equal(int32{1}, telemetry.num(), TEXT("Restart records one telemetry baseline"));
    checks.are_equal(uint64{0}, telemetry.last_time(), TEXT("Restart baseline uses tick zero"));
    checks.are_equal(test_driver->registry.get_num_alive_active_entities(),
                     telemetry.last_value(),
                     TEXT("Restart baseline records active entities"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FTestBatchOrchestratorResetScenario::check_reset() {
    ml::check_samples_recorded(initial_samples.num(), checks, TEXT("Initial samples recorded"));
    ml::check_samples_recorded(reset_samples.num(), checks, TEXT("Reset samples recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& reset_sample{reset_samples.last_value()};
    checks.is_true(reset_sample.actor_count < initial_actor_count,
                   TEXT("Reset removes transient level actors"));

    for (auto const& blocker : blockers) {
        checks.is_true(!ml::is_actor_in_world(*test_driver->get_world(), blocker.Get()),
                       TEXT("Reset blocker is removed"));
    }

    for (int32 i{0}; i < old_transient_actor_count; ++i) {
        checks.is_true(!ml::is_actor_in_world(*test_driver->get_world(), old_transient_actors[i]),
                       TEXT("Previous transient actor is removed"),
                       i);
    }

    auto const& orchestrator{test_driver->orchestrator};
    TStaticArray<AActor const*, owned_actor_count> const recreated_owned_actors{
        orchestrator.get_player_ship(),
        orchestrator.get_lasers(),
        orchestrator.get_capital_ships(),
        orchestrator.get_capital_ship_fighters(),
        orchestrator.get_turrets(),
        orchestrator.get_spinners(),
        orchestrator.get_niagara_spawner(),
    };
    for (int32 i{0}; i < owned_actor_count; ++i) {
        if (old_owned_actors[i] == nullptr) {
            checks.is_true(
                recreated_owned_actors[i] == nullptr, TEXT("Null owned actor remains null"), i);
            continue;
        }

        checks.is_true(IsValid(recreated_owned_actors[i]), TEXT("Owned actor is recreated"), i);
        checks.is_true(recreated_owned_actors[i] != old_owned_actors[i],
                       TEXT("Owned actor has a new address"),
                       i);
    }
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FTestBatchOrchestratorResetScenario::run() {
    TestCommandBuilder.Do([this] { start_initial_simulation(); })
        .Until([this] { return test_driver->get_time() >= reset_time; }, timeout)
        .Then([this] { reset_simulation(); })
        .Until([this] { return !reset_samples.is_empty(); }, timeout)
        .Then([this] { check_reset(); });
}
}
