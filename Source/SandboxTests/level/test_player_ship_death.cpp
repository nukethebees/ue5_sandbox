#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/time_series_test_data.h>
#include "test_player_ship_death_scenario.h"

#include <SandboxCore/time_series_data.h>

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestSpaceShipController.h>

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
    TestCommandBuilder.Do([this] { queue_player_ship_death(); })
        .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
        .Then([this] { check_player_ship_death(); });
}

void FTestPlayerShipDeathScenario::player_ship_pre_begin_play(UWorld& world,
                                                              UTestSimulationConfig const& config) {
    auto* const spawned_player_ship{
        ml::spawn_player_ship(world,
                              config.actor_classes.player_ship_class,
                              config.simulation_config->player_ship_config.Get())};
    if (!checks.is_valid(spawned_player_ship, TEXT("Player ship is spawned"))) {
        return;
    }

    player_ship = spawned_player_ship;
}

void FTestPlayerShipDeathScenario::player_ship_post_orchestrator_spawn(
    UWorld& world, UTestSimulationConfig const& config, ATestBatchOrchestrator& orchestrator) {
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

    TArray<FRegistryEntityHandle> const targets{player_ship_handle};
    test_driver->timeline
        .then_after(kill_time, [this, targets] { test_driver->queue_kills(targets); })
        .finish_after(post_kill_time);

    ml::reset_and_reserve_time_series(
        test_driver->orchestrator, kill_time + post_kill_time, samples);
    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FTestPlayerShipDeathScenario::on_end_tick));
}

void FTestPlayerShipDeathScenario::on_end_tick(ATestBatchOrchestrator&) {
    auto const& unique_entities{test_driver->registry.get_unique_entities()};

    if (!checks.is_true(unique_entities.alive.IsValidIndex(player_ship_id.id),
                        TEXT("Check player id is valid"))) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    samples.add(test_driver->get_time(),
                FSimulationSample{test_driver->registry.is_valid_dead(player_ship_handle),
                                  IsValid(player_ship.Get()),
                                  static_cast<bool>(unique_entities.alive[player_ship_id.id])});
    test_driver->advance_timeline();
}

void FTestPlayerShipDeathScenario::check_player_ship_death() {
    ml::check_samples_recorded(
        samples.num(), checks, TEXT("Player-death simulation samples recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{samples.last_value()};
    checks.is_true(sample.player_handle_is_dead, TEXT("Player ship handle is dead"));
    checks.is_true(!sample.player_actor_is_valid, TEXT("Player ship actor is destroyed"));
    checks.is_true(!sample.player_unique_entity_is_alive, TEXT("Player ship entity is dead"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}
}
