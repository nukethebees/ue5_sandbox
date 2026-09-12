#include "test_fighter_los_failure.h"

#include <SandboxTests/support/SoftTestAssertions.h>

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

namespace ml {
void run_worldless_fighter_los_failure(FAutomationTestBase& test,
                                       FSoftTestAssertions& checks,
                                       USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_spawns.add_defaulted(2);
    data.capital_spawns.locations.xs = {-39600.f, 50180.f};
    data.capital_spawns.locations.ys = {2170.f, 2170.f};
    data.capital_spawns.locations.zs = {4360.f, 4360.f};
    data.capital_spawns.teams = {ETestTeam::Blue, ETestTeam::Red};
    data.capital_spawns.healths = {data.capital_ships.max_health, data.capital_ships.max_health};
    data.capital_spawns.initial_spawn_delays = {0.f, 10000.f};
    data.capital_spawns.spawn_cooldowns = {1000.f, 10000.f};
    data.capital_target_spawn_indices = {1, 0};

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const enemy{capitals.get_handle(1)};
    auto const initial_enemy_health{capitals.get_health(enemy)};
    TArray<TArray<ETestTeam>> fighter_team_samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        TArray<ETestTeam> teams;
        teams.Append(fighters.get_teams());
        fighter_team_samples.Add(MoveTemp(teams));
    };
    harness.timeline.finish_at(30.0);
    test.TestTrue(TEXT("Fighter line-of-sight failure timeline completes"),
                  harness.run_until_timeline_finished(31.0));
    checks.is_true(!fighter_team_samples.IsEmpty(), TEXT("Simulation produced samples"));
    for (auto const& teams : fighter_team_samples) {
        check_all_teams_are(
            teams, ETestTeam::Blue, checks, TEXT("Only the blue hero team has fighters"));
    }
    check_health_decreased(initial_enemy_health,
                           capitals.get_health(enemy),
                           checks,
                           TEXT("Enemy capital has sustained damage by the end of the test"));
}

}
