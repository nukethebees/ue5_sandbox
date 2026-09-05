#include "test_fighter_los_failure_scenario.h"

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

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
    auto const* capitals{harness.get_simulation().get_capital_ships()};
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const enemy{capitals->get_handle(1)};
    auto const initial_enemy_health{capitals->get_health(enemy)};
    TArray<TArray<ETestTeam>> fighter_team_samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        TArray<ETestTeam> teams;
        teams.Append(fighters->get_teams());
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
                           capitals->get_health(enemy),
                           checks,
                           TEXT("Enemy capital has sustained damage by the end of the test"));
}

FFighterLosFailureScenario::FFighterLosFailureScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FFighterLosFailureScenario::spawn_fixture() {
    auto* const hero{spawn_capital_proxy(context_.world,
                                         context_.config,
                                         checks,
                                         TEXT("hero_capital"),
                                         FVector{-39600.f, 2170.f, 4360.f})};
    auto* const enemy_proxy{spawn_capital_proxy(context_.world,
                                                context_.config,
                                                checks,
                                                TEXT("enemy_capital"),
                                                FVector{50180.f, 2170.f, 4360.f})};
    if (!checks.is_valid(hero, TEXT("Hero capital is spawned")) ||
        !checks.is_valid(enemy_proxy, TEXT("Enemy capital is spawned"))) {
        return;
    }

    hero->set_team(hero_team);
    hero->set_target_ship(enemy_proxy);
    hero->set_spawn_cooldown(1000.f);
    enemy_proxy->set_team(enemy_team);
    enemy_proxy->set_target_ship(hero);
    enemy_proxy->set_initial_spawn_delay(10000.f);
    enemy_proxy->set_spawn_cooldown(10000.f);
}

void FFighterLosFailureScenario::initial_setup() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();

    auto const maybe_enemy{test_driver->get_capital_ships().find_first_handle_on_team(enemy_team)};
    if (checks.is_true(maybe_enemy.has_value(), TEXT("Find enemy capital"))) {
        enemy = *maybe_enemy;
        initial_enemy_health = test_driver->get_capital_ships().get_health(enemy);
    }

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    reset_and_reserve_time_series(test_driver->orchestrator, test_duration, samples);
    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FFighterLosFailureScenario::on_end_tick));
    test_driver->timeline.finish_at(test_duration);
}

/* ------------------------------------------------------------------------------------------ */
// Samples and checks
/* ------------------------------------------------------------------------------------------ */
void FFighterLosFailureScenario::sample_values() {
    FSimulationSample sample{};
    sample.fighter_teams.Append(test_driver->get_capital_ship_fighters().get_teams());
    samples.add(test_driver->get_time(), MoveTemp(sample));
}

void FFighterLosFailureScenario::on_end_tick(ATestBatchOrchestrator&) {
    sample_values();
    test_driver->advance_timeline();
}

void FFighterLosFailureScenario::check_fighter_spawns_and_survival() {
    for (auto const& sample : samples.values()) {
        check_all_teams_are(
            sample.fighter_teams, hero_team, checks, TEXT("Only the blue hero team has fighters"));
    }
}

void FFighterLosFailureScenario::full_checks() {
    check_samples_recorded(samples.num(), checks, TEXT("Simulation produced samples"));
    if (samples.is_empty()) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        return;
    }

    check_fighter_spawns_and_survival();
    auto const final_enemy_health{test_driver->get_capital_ships().get_health(enemy)};
    check_health_decreased(initial_enemy_health,
                           final_enemy_health,
                           checks,
                           TEXT("Enemy capital has sustained damage by the end of the test"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FFighterLosFailureScenario::run() {
    FTimespan const timeout{0, 0, static_cast<int32>(test_duration) + 1};
    run_until_timeline_finished([this] { initial_setup(); }, timeout, [this] { full_checks(); });
}
}
