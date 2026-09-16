#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/time_series_test_data.h>
#include "test_player_ship_death_scenario.h"

#include <sandbox/core/time_series_data.h>

#include <ioj/sim/entity_registry.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Misc/Optional.h>

namespace ml {
FTestPlayerShipDeathScenario::FTestPlayerShipDeathScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] {
        auto& world{context_.world};
        auto const& config{context_.config};
        player_ship_pre_begin_play(world, config);
        auto* const orchestrator{&context_.orchestrator};
        if (checks.is_valid(orchestrator, TEXT("Orchestrator is available"))) {
            player_ship_post_orchestrator_spawn(world, config, *orchestrator);
        }
    });
}

void FTestPlayerShipDeathScenario::run() {
    run_until_timeline_finished(
        [this] { queue_player_ship_death(); }, timeout, [this] { check_player_ship_death(); });
}

void FTestPlayerShipDeathScenario::player_ship_pre_begin_play(UWorld& world,
                                                              USpaceGameLevelConfig const& config) {
    auto* const spawned_player_ship{
        ml::spawn_player_ship(world, config.classes.player_ship_class, &config.player_ship)};
    if (!checks.is_valid(spawned_player_ship, TEXT("Player ship is spawned"))) {
        return;
    }

    player_ship = spawned_player_ship;
}

void FTestPlayerShipDeathScenario::player_ship_post_orchestrator_spawn(
    UWorld& world, USpaceGameLevelConfig const& config, ATestBatchOrchestrator& orchestrator) {
    if (!checks.is_true(player_ship.IsValid(), TEXT("Player ship is available"))) {
        return;
    }

    orchestrator.set_player_ship(*const_cast<ATestSpaceShip*>(player_ship.Get()));
}

void FTestPlayerShipDeathScenario::queue_player_ship_death() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();

    auto* const ship{test_driver->orchestrator.get_player_ship()};
    checks.is_valid(ship, TEXT("Player ship is valid"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    player_ship = const_cast<ATestSpaceShip*>(ship);
    player_ship_handle = ship->get_entity_handle();
    player_ship_id = ship->get_unique_id();

    auto const& registry{test_driver->orchestrator.get_entity_registry()};
    checks.is_true(registry.is_valid_handle(player_ship_handle),
                   TEXT("Player ship handle is valid"));
    checks.is_true(registry.is_valid_unique_id(player_ship_id), TEXT("Player ship ID is valid"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    std::vector<::ioj::sim::RegistryEntityHandle> const targets{player_ship_handle};
    test_driver->timeline.then_after(kill_time,
                                     [this, targets] { test_driver->queue_kills(targets); });

    begin_timed_sampling(
        kill_time + post_kill_time,
        FOrchestratorEndTickTestHook::CreateRaw(this, &FTestPlayerShipDeathScenario::on_end_tick),
        samples);
}

void FTestPlayerShipDeathScenario::on_end_tick(ATestBatchOrchestrator&) {
    auto const& unique_entities{test_driver->get_registry().get_unique_entities()};

    if (!checks.is_true(test_driver->get_registry().is_valid_unique_id(player_ship_id),
                        TEXT("Check player id is valid"))) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    auto const* const controller{
        Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    samples.add(
        test_driver->get_time(),
        FSimulationSample{test_driver->get_registry().is_valid_dead(player_ship_handle),
                          IsValid(player_ship.Get()),
                          unique_entities.life_state[test_driver->get_registry().get_history_index(
                              player_ship_id)] == ::ioj::sim::LifeState::Alive,
                          IsValid(controller) && IsValid(controller->GetPawn()),
                          IsValid(controller) && controller->get_active_control_context() ==
                                                     EPlayerControlContext::Player});
}

void FTestPlayerShipDeathScenario::check_player_ship_death() {
    ml::check_samples_recorded(
        samples.num(), checks, TEXT("Player-death simulation samples recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{samples.last_value()};
    checks.is_true(sample.player_handle_is_dead, TEXT("Player ship handle is dead"));
    checks.is_true(!sample.player_actor_is_valid, TEXT("Player ship actor is destroyed"));
    checks.is_true(!sample.player_unique_entity_is_alive, TEXT("Player ship entity is dead"));
    checks.is_true(!sample.controller_has_pawn, TEXT("Death unpossesses the controller"));
    checks.is_true(!sample.ship_control_active, TEXT("Death disables ship control"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}
}
