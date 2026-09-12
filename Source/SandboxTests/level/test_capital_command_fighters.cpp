#include "test_capital_command_fighters.h"

#include <SandboxTests/support/SoftTestAssertions.h>

#include <SandboxCoreEngine/enums.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>

#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

namespace ml {
void run_worldless_capital_command_fighters(FAutomationTestBase& test,
                                            FSoftTestAssertions& checks,
                                            USpaceGameLevelConfig const& config) {
    using Task = test_capital_ship_fighters::Simulation::Task;
    auto data{make_worldless_simulation_test_data(config)};
    add_worldless_capital_spawn(data, FVector3f{-18290.f, 2170.f, 4360.f}, ETestTeam::Green, 1);
    add_worldless_capital_spawn(data, FVector3f{17030.f, 2170.f, 4360.f}, ETestTeam::Red, 0);
    add_worldless_capital_spawn(data, FVector3f{17030.f, 12170.f, 4360.f}, ETestTeam::Red, 0);
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const first_target{capitals.get_handle(1)};
    FRegistryEntityHandle second_target;
    harness.timeline
        .then_after(2.0 / 60.0,
                    [&] {
                        checks.are_equal(first_target,
                                         capitals.get_target_handle(0),
                                         TEXT("Capital initially retains its configured target"));
                        checks.is_greater_than(capitals.get_fighter_handles(0).Num(),
                                               int32{0},
                                               TEXT("Main capital spawned fighters"));
                        harness.queue_kills(TArray{first_target});
                    })
        .then_after(2.0 / 60.0,
                    [&] {
                        second_target = capitals.get_target_handle(0);
                        checks.is_true(second_target.is_valid() && second_target != first_target,
                                       TEXT("Capital retargets after its first target dies"));
                        for (auto const fighter_handle : capitals.get_fighter_handles(0)) {
                            checks.are_equal(
                                second_target,
                                fighters.get_target_handle(fighter_handle),
                                TEXT("Fighter follows the replacement capital target"));
                        }
                        auto const enemies{
                            harness.get_registry().get_handles_not_in_team(ETestTeam::Green)};
                        harness.queue_kills(enemies);
                    })
        .then_after(2.0 / 60.0,
                    [&] {
                        for (auto const task : fighters.get_tasks()) {
                            checks.are_equal(
                                Task::Standby, task, TEXT("Fighter stands by with no enemies"));
                        }
                        checks.is_greater_than(capitals.get_num_instances(),
                                               int32{0},
                                               TEXT("The main capital remains alive"));
                    })
        .finish_after(0.0);
    test.TestTrue(TEXT("Capital fighter-command timeline completes"),
                  harness.run_until_timeline_finished(1.0));
}

}
