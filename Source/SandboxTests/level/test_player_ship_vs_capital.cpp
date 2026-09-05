#include "test_player_ship_vs_capital_scenario.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersConfig.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestPlayerShipVsCapitalResults.h>
#include <SandboxTests/support/TestResultAssetIO.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <Engine/DataTable.h>

namespace ml {
void run_worldless_player_ship_vs_capital(FAutomationTestBase& test,
                                          FSoftTestAssertions& checks,
                                          USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.laser.projectile_speed = 20000.f;
    data.fighters.laser.max_distance = 1.f;
    data.player.Emplace(make_worldless_player_spawn(
        config, FTransform{FRotator{0.f, -90.f, 0.f}, FVector{19850.f, 1300.f, 980.f}}));
    add_worldless_capital_spawn(data,
                                FVector3f{-22020.f, 2170.f, 4360.f},
                                ETestTeam::Green,
                                FLevelSimulationInitData::player_target_spawn_index,
                                0.f,
                                120.f);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto* const player{harness.get_simulation().get_player_ship_simulation()};
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    check(player);
    player->set_flight_mode(ETestSpaceShipFlightMode::ForwardSpeed);
    player->start_boost();
    auto const player_handle{player->registry_handle};
    struct Sample {
        FVector player_location;
        FVector registry_location;
        TArray<FVector3f> fighter_target_locations;
        TArray<FVector3f> fighter_locations;
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample{.player_location = player->transform.GetLocation(),
                      .registry_location =
                          FVector{harness.get_registry().get_location(player_handle)}};
        sample.fighter_target_locations = to_vector3f_array(fighters->get_target_locations());
        sample.fighter_locations = to_vector3f_array(fighters->get_locations());
        samples.add(harness.get_time(), MoveTemp(sample));
    };
    harness.timeline.finish_at(5.6);
    test.TestTrue(TEXT("Player-versus-capital timeline completes"),
                  harness.run_until_timeline_finished(6.0));
    checks.is_true(!samples.is_empty(), TEXT("Player-versus-capital samples are recorded"));
    if (samples.is_empty()) {
        return;
    }

    auto const& settled{samples.nearest_value(0.1)};
    auto const& tracked{samples.nearest_value(0.6)};
    auto const& before_end{samples.nearest_value(5.1)};
    auto const& end{samples.nearest_value(5.6)};
    checks.dist_zero(settled.player_location,
                     settled.registry_location,
                     1.0,
                     TEXT("Registry and player locations match initially"));
    checks.dist_zero(tracked.player_location,
                     tracked.registry_location,
                     1.0,
                     TEXT("Registry and player locations match after movement"));
    checks.not_dist_zero(
        settled.player_location, tracked.player_location, 1.0, TEXT("Player ship moves"));
    checks.is_greater_than(
        tracked.fighter_target_locations.Num(), int32{0}, TEXT("Fighters have target locations"));
    checks.are_equal(tracked.fighter_target_locations.Num(),
                     end.fighter_target_locations.Num(),
                     TEXT("Fighter target count remains stable"));
    checks.are_equal(end.fighter_target_locations.Num(),
                     end.fighter_locations.Num(),
                     TEXT("Fighter and target counts match"));
    if (!checks.all_passed) {
        return;
    }
    auto const count{end.fighter_locations.Num()};
    for (int32 i{}; i < count; ++i) {
        checks.not_dist_zero(tracked.fighter_target_locations[i],
                             end.fighter_target_locations[i],
                             1.f,
                             TEXT("Fighter target follows player"),
                             i);
        checks.dist_greater_than(before_end.fighter_locations[i],
                                 end.fighter_locations[i],
                                 500.f,
                                 TEXT("Fighter moves late in simulation"),
                                 i);
        checks.dist_greater_than(before_end.fighter_target_locations[i],
                                 end.fighter_target_locations[i],
                                 500.f,
                                 TEXT("Fighter target updates late in simulation"),
                                 i);
    }
}

FPlayerShipVsCapitalScenario::FPlayerShipVsCapitalScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FPlayerShipVsCapitalScenario::spawn_fixture() {
    auto* const level_config{duplicate_level_config(context_.config, context_.orchestrator)};
    if (!checks.not_nullptr(level_config, TEXT("Level config is duplicated"))) {
        return;
    }

    auto* const fighter_config{&level_config->fighters};
    if (!checks.not_nullptr(fighter_config,
                            TEXT("Player-versus-capital fighter config is created"))) {
        return;
    }
    fighter_config->laser.projectile_speed = 20000.f;
    fighter_config->laser.max_distance = 1.f;
    fighter_config->visual_logger_style = nullptr;
    context_.orchestrator.set_level_config(*level_config);

    auto* const player{spawn_player_ship(
        context_.world, context_.config.classes.player_ship_class, &context_.config.player_ship)};
    if (!checks.is_valid(player, TEXT("Player ship is spawned"))) {
        return;
    }
    player->set_flight_mode(ETestSpaceShipFlightMode::ForwardSpeed);
    player->SetActorTransform(
        FTransform{FRotator{0.f, -90.f, 0.f}, FVector{19850.f, 1300.f, 980.f}});
    context_.orchestrator.set_player_ship(*player);

    auto* const capital{spawn_capital_proxy(context_.world,
                                            context_.config,
                                            checks,
                                            TEXT("green_capital"),
                                            FVector{-22020.f, 2170.f, 4360.f})};
    if (!checks.is_valid(capital, TEXT("Capital is spawned"))) {
        return;
    }
    capital->set_team(ETestTeam::Green);
    capital->set_target_ship(player);
    capital->set_spawn_cooldown(120.f);
}

void FPlayerShipVsCapitalScenario::initial_setup() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();
    player_ship = &test_driver->get_player_ship();
    capitals = &test_driver->get_capital_ships();
    fighters = &test_driver->get_capital_ship_fighters();
    player_ship_handle = player_ship->get_entity_handle();
    reset_and_reserve_time_series(test_driver->orchestrator,
                                  t_end,
                                  player_ship_locations,
                                  player_ship_registry_locations,
                                  fighter_target_locations,
                                  fighter_locations,
                                  orchestrator_ticks);
    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FPlayerShipVsCapitalScenario::on_end_tick));
    test_driver->timeline.finish_at(t_end);
}

/* ------------------------------------------------------------------------------------------ */
// Samples and checks
/* ------------------------------------------------------------------------------------------ */
void FPlayerShipVsCapitalScenario::sample_values(ATestBatchOrchestrator& orchestrator) {
    auto const time{test_driver->get_time()};
    player_ship_locations.add(time, player_ship->GetActorLocation());
    player_ship_registry_locations.add(
        time, FVector{test_driver->get_registry().get_location(player_ship_handle)});
    fighter_target_locations.add(time, to_vector3f_array(fighters->get_target_locations()));
    fighter_locations.add(time, to_vector3f_array(fighters->get_locations()));
    orchestrator_ticks.add(time, orchestrator.get_completed_ticks());
}

void FPlayerShipVsCapitalScenario::on_end_tick(ATestBatchOrchestrator& orchestrator) {
    sample_values(orchestrator);
    test_driver->advance_timeline();
}

void FPlayerShipVsCapitalScenario::full_checks() {
    i_setup = fighter_target_locations.nearest_index(t_settled);
    i_tracked = fighter_target_locations.nearest_index(t_tracked);
    i_end = fighter_target_locations.nearest_index(t_end);
    i_before_end = fighter_target_locations.nearest_index(t_end - sample_time_before_end);

    checks.dist_zero(player_ship_locations.value_at(i_setup),
                     player_ship_registry_locations.value_at(i_setup),
                     1.0,
                     TEXT("Registry and ship locations same at start."));
    checks.dist_zero(player_ship_locations.value_at(i_tracked),
                     player_ship_registry_locations.value_at(i_tracked),
                     1.0,
                     TEXT("Registry and ship locations same after some time."));
    checks.not_dist_zero(player_ship_locations.value_at(i_setup),
                         player_ship_locations.value_at(i_tracked),
                         1.0,
                         TEXT("Ship moves"));
    checks.is_greater_than(num(fighter_target_locations.value_at(i_tracked)),
                           int32{0},
                           TEXT("Non-zero target locations"));

    auto const n_locations{fighter_target_locations.value_at(i_tracked).Num()};
    checks.are_equal(n_locations,
                     fighter_target_locations.value_at(i_end).Num(),
                     TEXT("Check num locations equal"));
    checks.are_equal(n_locations,
                     fighter_locations.value_at(i_end).Num(),
                     TEXT("Check num fighter locations same as target locations"));
    if (!checks.all_passed) {
        return;
    }

    for (int32 i{0}; i < n_locations; ++i) {
        checks.not_dist_zero(fighter_target_locations.value_at(i_tracked)[i],
                             fighter_target_locations.value_at(i_end)[i],
                             1.0,
                             TEXT("Check fighter target location updates"),
                             i);
    }

    constexpr double expected_min_distance_moved{500.0};
    for (int32 i{0}; i < n_locations; ++i) {
        checks.dist_greater_than(fighter_locations.value_at(i_before_end)[i],
                                 fighter_locations.value_at(i_end)[i],
                                 expected_min_distance_moved,
                                 TEXT("Check fighter moves late in sim"),
                                 i);
        checks.dist_greater_than(fighter_target_locations.value_at(i_before_end)[i],
                                 fighter_target_locations.value_at(i_end)[i],
                                 expected_min_distance_moved,
                                 TEXT("Check fighter target location updates late in sim"),
                                 i);
    }
}

void FPlayerShipVsCapitalScenario::fail_self_analysis() {
    export_failure_data();
    auto const times{player_ship_locations.times()};
    TestRunner->AddInfo(
        FString::Printf(TEXT("Sample indices and times: setup=%d (%.3f), tracked=%d (%.3f), "
                             "before_end=%d (%.3f), end=%d (%.3f)"),
                        i_setup,
                        times[i_setup],
                        i_tracked,
                        times[i_tracked],
                        i_before_end,
                        times[i_before_end],
                        i_end,
                        times[i_end]));
}

void FPlayerShipVsCapitalScenario::export_failure_data() const {
    auto const result_asset{FTestResultAsset{TEXT("player_ship_vs_capital"), *TestRunner}};
    auto* data_table{result_asset.load_or_create<UDataTable>(TEXT("data_table"))};
    data_table->EmptyTable();
    data_table->RowStruct = FPlayerShipVsCapitalResultRow::StaticStruct();
    auto const times{player_ship_locations.times()};
    for (int32 i{0}; i < times.Num(); ++i) {
        FPlayerShipVsCapitalResultRow row{};
        row.time = times[i];
        row.tick = orchestrator_ticks.value_at(i);
        row.player_ship_location = player_ship_locations.value_at(i);
        row.player_ship_registry_location = player_ship_registry_locations.value_at(i);
        for (auto const location : fighter_target_locations.value_at(i)) {
            row.fighter_target_locations.Add(FVector{location});
        }
        for (auto const location : fighter_locations.value_at(i)) {
            row.fighter_locations.Add(FVector{location});
        }
        data_table->AddRow(FName{FString::Printf(TEXT("%06d"), i)}, row);
    }
    result_asset.save(*data_table);
}

void FPlayerShipVsCapitalScenario::run() {
    TestCommandBuilder.Do([this] { initial_setup(); })
        .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
        .Do([this] {
            full_checks();
            if (!checks.all_passed) {
                fail_self_analysis();
            }
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        });
}
}
