#include "test_player_ship_vs_capital_scenario.h"

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipFightersConfig.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestSpaceShip.h>

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestPlayerShipVsCapitalResults.h>
#include <SandboxTests/support/TestResultAssetIO.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <Engine/DataTable.h>
#include <UObject/SoftObjectPath.h>

namespace ml {
FPlayerShipVsCapitalScenario::FPlayerShipVsCapitalScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FPlayerShipVsCapitalScenario::tear_down() {
    if (test_driver.IsSet()) {
        test_driver->orchestrator.clear_end_tick_test_hook();
    }
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FPlayerShipVsCapitalScenario::spawn_fixture() {
    auto* const fighter_config{Cast<UTestCapitalShipFightersConfig>(
        FSoftObjectPath{FLevelTestConfigPaths::player_ship_vs_capital_fighter_config}.TryLoad())};
    auto* const fighter_actor{
        const_cast<ATestCapitalShipFighters*>(context_.orchestrator.get_capital_ship_fighters())};
    auto const* const simulation_config{context_.config.simulation_config.Get()};
    if (!checks.not_nullptr(fighter_config,
                            TEXT("Player-versus-capital fighter config is loaded")) ||
        !checks.is_valid(fighter_actor, TEXT("Fighter batch actor is available")) ||
        !checks.not_nullptr(simulation_config, TEXT("Simulation config is available"))) {
        return;
    }
    fighter_actor->set_actor_config(fighter_config);

    auto* const player{spawn_player_ship(context_.world,
                                         context_.config.actor_classes.player_ship_class,
                                         simulation_config->player_ship_config)};
    if (!checks.is_valid(player, TEXT("Player ship is spawned"))) {
        return;
    }
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
    test_driver = TestSimulationDriver::from_world(context_.world);
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
        time, FVector{test_driver->registry.get_location(player_ship_handle)});
    fighter_target_locations.add(time, to_vector3f_array(fighters->get_target_locations()));
    fighter_locations.add(time, to_vector3f_array(fighters->get_locations()));
    orchestrator_ticks.add(time, orchestrator.get_completed_ticks());
}

void FPlayerShipVsCapitalScenario::on_end_tick(ATestBatchOrchestrator& orchestrator) {
    sample_values(orchestrator);
    test_driver->timeline.tick(test_driver->get_time());
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
