#include "test_player_ship_death_scenario.h"
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/time_series_data.h>
#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Misc/Optional.h>

namespace ioj {
FTestPlayerShipDeathScenario::FTestPlayerShipDeathScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

void FTestPlayerShipDeathScenario::run() {
    run_until_timeline_finished(
        [this] { queue_player_ship_death(); }, timeout, [this] { check_player_ship_death(); });
}

void FTestPlayerShipDeathScenario::queue_player_ship_death() {
    initialise_test_driver();
    if (!load_player_test_level(test_driver->orchestrator, checks)) {
        return;
    }
    test_driver->orchestrator.start_simulation();

    auto* const ship{test_driver->orchestrator.get_player_ship()};
    checks.is_valid(ship, TEXT("Player ship is valid"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    player_ship = const_cast<ATestSpaceShip*>(ship);
    player_ship_id = ship->get_unique_id();

    auto const& ledger{test_driver->orchestrator.get_level_simulation()->entity_ledger()};
    checks.is_true(ledger.is_valid_unique_id(player_ship_id), TEXT("Player ship ID is valid"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    std::vector<sim::EntityId> const targets{player_ship_id};
    test_driver->timeline.then_after(kill_time,
                                     [this, targets] { test_driver->queue_kills(targets); });

    begin_timed_sampling(
        kill_time + post_kill_time,
        FSimulationSampleCallback::CreateRaw(this, &FTestPlayerShipDeathScenario::on_end_tick),
        samples);
}

void FTestPlayerShipDeathScenario::on_end_tick(ATestBatchOrchestrator&) {
    auto const& unique_entities{test_driver->get_ledger().unique_entities()};

    if (!checks.is_true(test_driver->get_ledger().is_valid_unique_id(player_ship_id),
                        TEXT("Check player id is valid"))) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    auto const* const controller{
        Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    samples.add(
        test_driver->get_time(),
        FSimulationSample{
            !sim::entity_is_alive(*test_driver->orchestrator.get_level_simulation(),
                                  player_ship_id),
            IsValid(player_ship.Get()),
            unique_entities.life_state()[test_driver->get_ledger().history_index(player_ship_id)] ==
                sim::LifeState::Alive,
            IsValid(controller) && IsValid(controller->GetPawn()),
            IsValid(controller) &&
                controller->get_active_control_context() == ::ioj::levels::ControlContext::Player});
}

void FTestPlayerShipDeathScenario::check_player_ship_death() {
    check_samples_recorded(samples.num(), checks, TEXT("Player-death simulation samples recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{samples.last_value()};
    checks.is_true(sample.player_id_is_dead, TEXT("Player ship ID is dead"));
    checks.is_true(!sample.player_actor_is_valid, TEXT("Player ship actor is destroyed"));
    checks.is_true(!sample.player_unique_entity_is_alive, TEXT("Player ship entity is dead"));
    checks.is_true(!sample.controller_has_pawn, TEXT("Death unpossesses the controller"));
    checks.is_true(!sample.ship_control_active, TEXT("Death disables ship control"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}
}
