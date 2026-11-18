#include "test_player_ship_death_scenario.h"
#include <ioj/sim/testing/entity_observations.h>
#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <Engine/World.h>

#include <array>

namespace ioj {
FTestPlayerShipDeathScenario::FTestPlayerShipDeathScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

void FTestPlayerShipDeathScenario::run() {
    TestCommandBuilder.Do([this] {
        auto& driver{initialise_test_driver()};
        auto& orchestrator{driver.orchestrator};
        if (!load_player_test_level(orchestrator, checks)) {
            return;
        }
        orchestrator.start_simulation();
        orchestrator.SetActorTickEnabled(false);
        auto const* const ship{orchestrator.get_player_ship()};
        checks.is_valid(ship, TEXT("Player ship is valid"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        TWeakObjectPtr<ATestSpaceShip const> const player_ship{ship};
        auto const player_id{ship->get_unique_id()};
        auto const& simulation{*orchestrator.get_level_simulation()};
        auto const& ledger{simulation.entity_ledger()};
        checks.is_true(ledger.is_valid_unique_id(player_id), TEXT("Player ship ID is valid"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        driver.queue_kills(std::array{player_id});
        orchestrator.tick(simulation.clock().tick_period());
        checks.is_true(ledger.is_valid_unique_id(player_id), TEXT("Dead player ID remains valid"));
        checks.is_true(!sim::entity_is_alive(simulation, player_id),
                       TEXT("Player ship ID is dead"));
        checks.is_true(!player_ship.IsValid(), TEXT("Player ship actor is destroyed"));
        checks.is_true(ledger.history().life_state()[ledger.history_index(player_id)] !=
                           sim::LifeState::Alive,
                       TEXT("Player ship entity is dead"));
        auto const* const controller{
            Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
        checks.is_valid(controller, TEXT("Player controller remains available"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        checks.is_true(!IsValid(controller->GetPawn()), TEXT("Death unpossesses the controller"));
        checks.is_true(controller->get_active_control_context() != levels::ControlContext::Player,
                       TEXT("Death disables ship control"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    });
}
}
