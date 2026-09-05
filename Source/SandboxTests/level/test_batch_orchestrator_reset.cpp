#include <SandboxISMCComponent.h>
#include <SandboxTests/support/test_setup.h>
#include "test_batch_orchestrator_reset_scenario.h"

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersSimulation.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsSimulation.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

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
    TestCommandBuilder.Do([this] { spawn_blockers(context_.world); });
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

    auto* const capital{spawn_capital_proxy(
        world, context_.config, checks, TEXT("reset_capital"), FVector{5000.f, 0.f, 0.f})};
    if (IsValid(capital)) {
        capital->set_initial_spawn_delay(60.f);
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
        .registry_alive = test_driver->get_registry().count_alive(),
        .capital_count = orchestrator.get_capital_ships()->get_num_instances(),
        .fighter_count = orchestrator.get_capital_ship_fighters()->get_num_instances(),
        .laser_count = orchestrator.get_lasers()->get_num_instances(),
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
    initialise_test_driver();
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
    initial_registry_alive = initial_samples.last_value().registry_alive;
    save_old_owned_actors(test_driver->orchestrator);
    save_old_transient_actors(test_driver->orchestrator);
    auto const& initial_telemetry{
        test_driver->orchestrator.get_level_telemetry_manager().get_active_entity_count_data()};
    checks.is_true(!initial_telemetry.is_empty(), TEXT("Initial level telemetry is populated"));
    auto const resources{test_driver->orchestrator.get_presentation_resources()};
    test_driver->orchestrator.reset_for_new_level();
    auto const retained{test_driver->orchestrator.get_presentation_resources()};
    checks.is_true(
        resources.lasers == retained.lasers && resources.capital_ships == retained.capital_ships &&
            resources.fighters == retained.fighters && resources.turrets == retained.turrets &&
            resources.spinners == retained.spinners,
        TEXT("Reset retains the orchestrator's presentation components"));
    checks.are_equal(
        0, retained.lasers->get_instance_count(), TEXT("Reset clears laser instances"));
    for (auto const* component :
         {retained.capital_ships, retained.fighters, retained.turrets, retained.spinners}) {
        checks.are_equal(
            0, component->GetInstanceCount(), TEXT("Reset clears presentation instances"));
    }
    checks.is_true(test_driver->orchestrator.get_level_simulation() == nullptr,
                   TEXT("Level reset destroys the previous simulation"));
    reset_complete = true;
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTestBatchOrchestratorResetScenario::on_end_tick));
    test_driver->orchestrator.start_simulation();
    auto const& telemetry{
        test_driver->orchestrator.get_level_telemetry_manager().get_active_entity_count_data()};
    checks.are_equal(int32{1}, telemetry.num(), TEXT("Restart records one telemetry baseline"));
    checks.are_equal(uint64{0}, telemetry.last_time(), TEXT("Restart baseline uses tick zero"));
    checks.are_equal(test_driver->get_registry().get_num_alive_active_entities(),
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
    checks.are_equal(1, initial_registry_alive, TEXT("Initial simulation contains one entity"));
    checks.are_equal(0, reset_sample.registry_alive, TEXT("Reset clears registry entities"));
    checks.are_equal(0, reset_sample.capital_count, TEXT("Reset clears capital instances"));
    checks.are_equal(0, reset_sample.fighter_count, TEXT("Reset clears fighter instances"));
    checks.are_equal(0, reset_sample.laser_count, TEXT("Reset clears projectile instances"));

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
