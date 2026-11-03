#include "test_batch_orchestrator_reset_scenario.h"
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/lasers/sim.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

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
#include <SandboxISMCComponent.h>

namespace ioj {
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
    return actor_is_any<AWorldSettings,
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

void FTestBatchOrchestratorResetScenario::start_initial_simulation() {
    initialise_test_driver();
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    test_driver->orchestrator.start_simulation();
    test_driver->orchestrator.SetActorTickEnabled(false);
}

void FTestBatchOrchestratorResetScenario::reset_simulation() {
    initial_actor_count = count_actors(*test_driver->get_world());
    initial_ledger_alive = test_driver->get_ledger().count_alive();
    save_old_owned_actors(test_driver->orchestrator);
    save_old_transient_actors(test_driver->orchestrator);
    {
        auto const* const level{test_driver->orchestrator.get_level_simulation()};
        check(level);
        checks.is_true(level->level_telemetry_manager().recorded_sample_count() != 0,
                       TEXT("Initial level telemetry is populated"));
    }
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
    checks.are_equal(
        0, retained.fighters->get_instance_count(), TEXT("Reset clears fighter instances"));
    for (auto const* component : {retained.capital_ships, retained.turrets, retained.spinners}) {
        checks.are_equal(
            0, component->GetInstanceCount(), TEXT("Reset clears presentation instances"));
    }
    checks.is_true(test_driver->orchestrator.get_level_simulation() == nullptr,
                   TEXT("Level reset destroys the previous simulation"));
    test_driver->orchestrator.start_simulation();
    test_driver->orchestrator.SetActorTickEnabled(false);
    auto const* const level{test_driver->orchestrator.get_level_simulation()};
    check(level);
    auto const history{level->level_telemetry_manager().materialize_history()};
    checks.are_equal(
        std::size_t{1}, history.size(), TEXT("Restart records one telemetry baseline"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const& baseline{history.front()};
    checks.are_equal(
        uint64{0}, baseline.completed_tick.value(), TEXT("Restart baseline uses tick zero"));
    checks.are_equal(test_driver->get_ledger().count_alive(),
                     sim::telemetry::total_count(baseline.alive),
                     TEXT("Restart baseline records active entities"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FTestBatchOrchestratorResetScenario::check_reset() {
    auto const* const level{test_driver->orchestrator.get_level_simulation()};
    check(level);
    checks.is_true(count_actors(*test_driver->get_world()) < initial_actor_count,
                   TEXT("Reset removes transient level actors"));
    checks.are_equal(1, initial_ledger_alive, TEXT("Initial simulation contains one entity"));
    checks.are_equal(
        0, test_driver->get_ledger().count_alive(), TEXT("Reset clears ledger entities"));
    checks.are_equal(
        0u, level->capital_ships().num_instances(), TEXT("Reset clears capital instances"));
    checks.are_equal(0u, level->fighters().num_instances(), TEXT("Reset clears fighter instances"));
    checks.are_equal(
        0u, level->lasers().num_instances(), TEXT("Reset clears projectile instances"));

    for (auto const& blocker : blockers) {
        checks.is_true(!is_actor_in_world(*test_driver->get_world(), blocker.Get()),
                       TEXT("Reset blocker is removed"));
    }

    for (int32 i{0}; i < old_transient_actor_count; ++i) {
        checks.is_true(!is_actor_in_world(*test_driver->get_world(), old_transient_actors[i]),
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
        .Until(
            [this] {
                advance_sample();
                return test_driver->get_time() >= reset_time;
            },
            timeout)
        .Then([this] { reset_simulation(); })
        .Then([this] {
            advance_sample();
            check_reset();
        });
}
}
