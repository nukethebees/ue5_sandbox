#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include "test_hud_manager_scenario.h"

#include <SandboxCore/time_series_data.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <SandboxGameShared/utilities/enums.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/presentation/HUDManager.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/presentation/widgets/ShipHudWidget.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Optional.h>

namespace ml {
namespace {
auto count_worldless_hud_entities(FHUDManager const& manager) -> int32 {
    int32 count{};
    for (auto const& team : manager.get_entity_count_data().alive_per_team_and_type) {
        for (auto const value : team) {
            count += value;
        }
    }
    return count;
}
}

void run_worldless_hud_manager_scenario(FAutomationTestBase& test,
                                        FSoftTestAssertions& checks,
                                        USpaceGameLevelConfig const& config,
                                        EHUDManagerScenario const scenario) {
    check(scenario != EHUDManagerScenario::LateHUDRegistrationSynchronisesAndUnregisters);
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    auto const needs_defence{
        scenario == EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD ||
        scenario == EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD};
    auto const needs_player{scenario == EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD};
    if (needs_player) {
        data.player.Emplace(make_worldless_player_spawn(config));
    }
    int32 first_capital_index{INDEX_NONE};
    int32 second_capital_index{INDEX_NONE};
    if (scenario == EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD || needs_player) {
        first_capital_index = add_worldless_capital_spawn(
            data, FVector3f{2000.f, 0.f, 0.f}, ETestTeam::Red, INDEX_NONE, 60.f, 60.f);
    } else if (needs_defence) {
        first_capital_index = add_worldless_capital_spawn(
            data, FVector3f::ZeroVector, ETestTeam::Blue, INDEX_NONE, 60.f, 60.f);
        second_capital_index = add_worldless_capital_spawn(
            data, FVector3f{2000.f, 0.f, 0.f}, ETestTeam::Red, INDEX_NONE, 60.f, 60.f);
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    auto& simulation{harness.get_simulation()};
    auto& mission{simulation.get_mission_manager()};
    auto const* capitals{simulation.get_capital_ships()};
    auto const first_capital{first_capital_index == INDEX_NONE
                                 ? FRegistryEntityHandle{}
                                 : capitals->get_handle(first_capital_index)};
    auto const second_capital{second_capital_index == INDEX_NONE
                                  ? FRegistryEntityHandle{}
                                  : capitals->get_handle(second_capital_index)};
    if (needs_defence) {
        mission.set_save_mission_results(false);
        mission.set_mission_mode(ETestMissionMode::SurviveTime);
        mission.set_target_time(10.f);
        mission.add_entity_that_must_survive(first_capital);
        mission.add_entity_required_to_kill(second_capital);
    } else {
        mission.set_mission_mode(ETestMissionMode::None);
        mission.set_save_mission_results(true);
    }
    harness.finish_initialisation();

    FHUDManager hud;
    hud.initialise(FTestBatchGameUiUpdateFrequencies{},
                   mission,
                   harness.get_registry(),
                   60.0,
                   simulation.get_player_ship_simulation(),
                   config,
                   {});
    checks.are_equal(0, hud.get_registered_hud_count(), TEXT("No HUD widgets are registered"));
    checks.are_equal(EHUDManagerState::Active, hud.get_state(), TEXT("HUD manager is active"));
    checks.are_equal(harness.get_registry().get_num_alive_active_entities(),
                     count_worldless_hud_entities(hud),
                     TEXT("Initial entity cache matches registry"));
    checks.are_equal(mission.get_mission_state(),
                     hud.get_mission_data().status_data.mission_state,
                     TEXT("Mission state is cached"));
    if (scenario == EHUDManagerScenario::InitialCachesPopulateWithoutHUD) {
        return;
    }

    auto const initial_count{count_worldless_hud_entities(hud)};
    if (scenario == EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD) {
        harness.timeline.then_after(0.1, [&] { harness.queue_kills(TArray{first_capital}); });
    } else if (scenario == EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD) {
        harness.timeline.then_after(0.1, [&] {
            harness.queue_kills(TArray{second_capital});
            harness.queue_kills(TArray{first_capital});
        });
    } else if (needs_player) {
        auto const player_handle{simulation.get_player_ship_simulation()->registry_handle};
        harness.timeline.then_after(
            0.1, [&] { harness.queue_kills(TArray{first_capital}, player_handle); });
    }
    harness.on_end_tick = [&](FLevelSimulation&) { hud.force_sample(); };
    harness.timeline.finish_at(0.35);
    test.TestTrue(TEXT("HUD cache timeline completes"), harness.run_until_timeline_finished(1.0));

    if (scenario == EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD) {
        checks.are_equal(initial_count - 1,
                         count_worldless_hud_entities(hud),
                         TEXT("Entity cache updates without a HUD"));
    } else if (scenario == EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD) {
        auto const& status{hud.get_mission_data().status_data};
        checks.are_equal(
            ETestMissionState::Failed, status.mission_state, TEXT("Defence failure is cached"));
        checks.is_true(status.surviving_entity_health[0].health <= 0,
                       TEXT("Destroyed survivor health is cached"));
        checks.is_true(status.required_kill_entity_health[0].health <= 0,
                       TEXT("Destroyed required-kill health is cached"));
    } else if (needs_player) {
        auto const& player_status{hud.get_player_status_data()};
        checks.is_true(player_status.has_player_ship, TEXT("Player HUD state is available"));
        checks.are_equal(1, player_status.points, TEXT("Player kill count is cached"));
        checks.are_equal(1,
                         harness.get_registry().count_kills(),
                         TEXT("Kill data source records the player kill"));
    } else {
        auto const cached_time{hud.get_mission_data().status_data.mission_stopwatch};
        checks.is_true(cached_time > 0.f, TEXT("Cached mission time advances"));
        checks.is_true(mission.get_mission_stopwatch() - cached_time <= 0.05f,
                       TEXT("Cached mission time follows simulation time"));
    }
}

FTestHUDManagerScenario::FTestHUDManagerScenario(FSimulationTestContext& context,
                                                 EHUDManagerScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {
    TestCommandBuilder.Do([this] {
        auto* const orchestrator{&context_.orchestrator};
        if (!checks.is_valid(orchestrator, TEXT("Orchestrator is available"))) {
            return;
        }

        auto& mission_manager{orchestrator->get_mission_definition()};
        mission_manager.set_mission_mode(ETestMissionMode::None);
        mission_manager.set_save_mission_results(true);
    });
}

void FTestHUDManagerScenario::on_tear_down() {
    check_headless_hud_manager_matches_simulation();
    ml::reset(checks, test_driver, headless_hud_manager);
}

/* ------------------------------------------------------------------------------------------ */
// Initial cache
/* ------------------------------------------------------------------------------------------ */
void FTestHUDManagerScenario::initial_caches_process_samples() {
    if (!initialise_headless_hud_manager()) {
        return;
    }

    auto* const orchestrator{&context_.orchestrator};
    if (!checks.not_nullptr(orchestrator, TEXT("Orchestrator is available"))) {
        return;
    }

    auto const& hud_manager{get_headless_hud_manager()};
    auto const& registry{orchestrator->get_entity_registry()};
    auto const& mission_manager{orchestrator->get_mission_manager()};
    auto const* const player_ship{orchestrator->get_player_ship()};

    checks.are_equal(
        0, hud_manager.get_registered_hud_count(), TEXT("No HUD widgets are registered"));
    checks.are_equal(
        EHUDManagerState::Active, hud_manager.get_state(), TEXT("HUD manager is active"));
    checks.are_equal(registry.get_num_alive_active_entities(),
                     count_cached_entities(hud_manager),
                     TEXT("Initial entity count cache matches the registry"));

    auto const& mission_data{hud_manager.get_mission_data()};
    checks.are_equal(mission_manager.get_mission_state(),
                     mission_data.status_data.mission_state,
                     TEXT("Mission state is cached"));
    checks.are_equal(mission_manager.get_mission_stopwatch(),
                     mission_data.status_data.mission_stopwatch,
                     TEXT("Mission time is cached"));

    if (IsValid(player_ship)) {
        auto const& player_data{hud_manager.get_player_status_data()};
        checks.is_true(player_data.has_player_ship, TEXT("Player HUD state is available"));
        auto const player_health{player_ship->get_health_info()};
        checks.are_equal(player_health.health,
                         player_data.health.health,
                         TEXT("Player current health is cached"));
        checks.are_equal(player_health.max_health,
                         player_data.health.max_health,
                         TEXT("Player maximum health is cached"));
        checks.are_equal(
            player_ship->get_speed(), player_data.speed, TEXT("Player speed is cached"));
        checks.are_equal(player_ship->get_target_speed(),
                         player_data.target_speed,
                         TEXT("Player target speed is cached"));
    }

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

/* ------------------------------------------------------------------------------------------ */
// Defence mission
/* ------------------------------------------------------------------------------------------ */
void FTestHUDManagerScenario::defence_pre_begin_play(UWorld& world,
                                                     USpaceGameLevelConfig const& config) {
    auto* const defended{ml::spawn_capital_proxy(
        world, config, checks, FName{TEXT("defended_capital")}, FVector::ZeroVector)};
    if (!IsValid(defended)) {
        return;
    }
    ml::spawn_capital_proxy(
        world, config, checks, FName{TEXT("required_enemy_capital")}, FVector{2000.f, 0.f, 0.f});
}

void FTestHUDManagerScenario::configure_defence_mission(UWorld& world,
                                                        ATestBatchOrchestrator& orchestrator) {
    auto const capitals{ml::get_actors<ATestCapitalShipProxy>(world)};
    auto const find_capital{[&capitals](FName const test_name) {
        auto* const* const capital{
            capitals.FindByPredicate([test_name](ATestCapitalShipProxy const* const candidate) {
                return candidate->get_test_name() == test_name;
            })};
        check(capital);
        return *capital;
    }};
    auto* const defended{find_capital(FName{TEXT("defended_capital")})};
    auto* const required_enemy{find_capital(FName{TEXT("required_enemy_capital")})};

    auto& mission_manager{orchestrator.get_mission_definition()};
    mission_manager.set_save_mission_results(false);
    mission_manager.set_mission_mode(ETestMissionMode::SurviveTime);
    mission_manager.set_target_time(10.f);
    mission_manager.add_entity_that_must_survive(*defended);
    mission_manager.add_entity_required_to_kill(*required_enemy);
}

void FTestHUDManagerScenario::defence_begin() {
    initialise_test_driver();
    if (!initialise_headless_hud_manager()) {
        return;
    }
    auto const& initial_data{get_headless_hud_manager().get_mission_data()};
    checks.are_equal(
        1, initial_data.static_data.surviving_entity_ids.Num(), TEXT("Must-survive ID is cached"));
    checks.are_equal(1,
                     initial_data.static_data.surviving_entity_types.Num(),
                     TEXT("Must-survive type is cached"));
    checks.are_equal(1,
                     initial_data.status_data.surviving_entity_health.Num(),
                     TEXT("Must-survive health is cached"));
    checks.is_true(initial_data.status_data.surviving_entity_health[0].health > 0,
                   TEXT("Must-survive entity starts healthy"));
    checks.are_equal(1,
                     initial_data.static_data.required_kill_entity_ids.Num(),
                     TEXT("Required-kill ID is cached"));
    checks.are_equal(1,
                     initial_data.static_data.required_kill_entity_types.Num(),
                     TEXT("Required-kill type is cached"));
    checks.are_equal(1,
                     initial_data.status_data.required_kill_entity_health.Num(),
                     TEXT("Required-kill health is cached"));
    checks.is_true(initial_data.status_data.required_kill_entity_health[0].health > 0,
                   TEXT("Required-kill entity starts healthy"));

    auto const handles{
        test_driver->orchestrator.get_mission_manager().get_entity_handles_that_must_survive()};
    check(handles.Num() == 1);
    auto const required_handles{
        test_driver->orchestrator.get_mission_manager().get_entity_handles_required_to_kill()};
    check(required_handles.Num() == 1);
    test_driver->timeline.then_after(damage_queue_time, [this, handles, required_handles] {
        test_driver->queue_kills(required_handles);
        test_driver->queue_kills(handles);
    });
    ml::reset_and_reserve_time_series(test_driver->orchestrator, test_duration, defence_samples);
    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FTestHUDManagerScenario::defence_on_tick));
    test_driver->timeline.finish_at(test_duration);
}

void FTestHUDManagerScenario::defence_on_tick(ATestBatchOrchestrator&) {
    tick_headless_hud_manager();
    auto const& data{get_headless_hud_manager().get_mission_data()};
    FDefenceSample sample{};
    sample.mission_state = data.status_data.mission_state;
    sample.mission_stopwatch = data.status_data.mission_stopwatch;
    sample.registered_hud_count =
        test_driver->orchestrator.get_hud_manager().get_registered_hud_count();
    if (data.status_data.surviving_entity_health.Num() == 1) {
        sample.defended_entity_health = data.status_data.surviving_entity_health[0].health;
    }
    if (data.status_data.required_kill_entity_health.Num() == 1) {
        sample.required_kill_entity_health = data.status_data.required_kill_entity_health[0].health;
    }
    defence_samples.add(test_driver->get_time(), sample);
    test_driver->advance_timeline();
}

void FTestHUDManagerScenario::defence_process_samples() {
    ml::check_samples_recorded(defence_samples.num(), checks, TEXT("Defence samples recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{defence_samples.nearest_value(test_duration)};
    checks.are_equal(
        ETestMissionState::Failed, sample.mission_state, TEXT("Defence mission failure is cached"));
    checks.is_less_equal_than(
        sample.defended_entity_health, 0, TEXT("Destroyed must-survive health is cached"));
    checks.is_less_equal_than(
        sample.required_kill_entity_health, 0, TEXT("Destroyed required-kill health is cached"));
    checks.is_true(sample.mission_stopwatch > 0.f,
                   TEXT("Mission stopwatch follows simulation time"));
    checks.are_equal(0, sample.registered_hud_count, TEXT("Mission updates without a HUD"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

/* ------------------------------------------------------------------------------------------ */
// Timed mission
/* ------------------------------------------------------------------------------------------ */
void FTestHUDManagerScenario::mission_time_pre_begin_play(UWorld& world,
                                                          USpaceGameLevelConfig const& config) {
    defence_pre_begin_play(world, config);
}

void FTestHUDManagerScenario::mission_time_begin() {
    initialise_test_driver();
    if (!initialise_headless_hud_manager()) {
        return;
    }
    ml::reset_and_reserve_time_series(
        test_driver->orchestrator, test_duration, mission_time_samples);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTestHUDManagerScenario::mission_time_on_tick));
    test_driver->timeline.finish_at(test_duration);
}

void FTestHUDManagerScenario::mission_time_on_tick(ATestBatchOrchestrator& orchestrator) {
    tick_headless_hud_manager();
    auto const& mission_manager{orchestrator.get_mission_manager()};

    auto const& hud_manager{get_headless_hud_manager()};
    auto const& mission_data{hud_manager.get_mission_data()};
    mission_time_samples.add(test_driver->get_time(),
                             FMissionTimeSample{mission_data.status_data.mission_stopwatch,
                                                mission_manager.get_mission_stopwatch(),
                                                hud_manager.get_registered_hud_count()});
    test_driver->advance_timeline();
}

void FTestHUDManagerScenario::mission_time_process_samples() {
    ml::check_samples_recorded(
        mission_time_samples.num(), checks, TEXT("Mission-time samples recorded"));
    if (mission_time_samples.is_empty()) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        return;
    }

    auto const& sample{mission_time_samples.nearest_value(test_duration)};

    checks.is_true(sample.cached_time > 0.f, TEXT("Cached mission time advances"));
    checks.is_true(sample.mission_time - sample.cached_time <= 0.3f,
                   TEXT("Cached mission time follows the mission manager cadence"));
    checks.is_true(mission_time_samples.nearest_time(test_duration) - sample.cached_time <= 0.3,
                   TEXT("Cached mission time follows simulation-clock cadence"));
    checks.are_equal(0, sample.registered_hud_count, TEXT("Mission time updates without a HUD"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

/* ------------------------------------------------------------------------------------------ */
// Player kill
/* ------------------------------------------------------------------------------------------ */
void FTestHUDManagerScenario::player_kill_pre_begin_play(UWorld& world,
                                                         USpaceGameLevelConfig const& config) {
    auto* const player_ship{
        ml::spawn_player_ship(world, config.classes.player_ship_class, &config.player_ship)};
    if (!checks.is_valid(player_ship, TEXT("Player ship is spawned"))) {
        return;
    }

    ml::spawn_capital_proxy(world,
                            config,
                            checks,
                            FName{TEXT("player_kill_target_capital")},
                            FVector{2000.f, 0.f, 0.f});
}

void FTestHUDManagerScenario::player_kill_begin() {
    initialise_test_driver();

    auto* player_ship{ml::get_first_actor<ATestSpaceShip>(test_driver->world)};
    if (!checks.is_true(IsValid(player_ship), TEXT("Got player ship"))) {
        return;
    }
    test_driver->orchestrator.set_player_ship(*player_ship);

    if (!initialise_headless_hud_manager()) {
        return;
    }
    auto const& capitals{test_driver->get_capital_ships()};
    check(capitals.get_num_instances() == 1);

    TArray<FRegistryEntityHandle> const targets{capitals.get_handle(0)};
    auto const instigator{player_ship->get_entity_handle()};
    test_driver->timeline.then_after(damage_queue_time, [this, targets, instigator] {
        test_driver->queue_kills(targets, instigator);
    });
    ml::reset_and_reserve_time_series(test_driver->orchestrator, test_duration, player_samples);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTestHUDManagerScenario::player_kill_on_tick));
    test_driver->timeline.finish_at(test_duration);
}

void FTestHUDManagerScenario::player_kill_on_tick(ATestBatchOrchestrator&) {
    tick_headless_hud_manager();
    auto const& hud_manager{get_headless_hud_manager()};
    auto const& data{hud_manager.get_player_status_data()};
    auto const& player_ship{test_driver->get_player_ship()};
    auto const& kill_data{hud_manager.get_kill_data()};
    int32 top_killer_kills{0};
    auto const n_top_killers{kill_data.top_killers.num()};
    for (int32 i{0}; i < n_top_killers; ++i) {
        if (kill_data.top_killers.entity_ids[i] == player_ship.get_unique_id()) {
            top_killer_kills = kill_data.top_killers.kills[i];
            break;
        }
    }

    auto const player_team_row{kill_data.team_kill_matrix.get_team_kills(player_ship.get_team())};
    int32 player_team_matrix_kills{0};
    for (int32 const kills : player_team_row) {
        player_team_matrix_kills += kills;
    }

    player_samples.add(test_driver->get_time(),
                       FPlayerSample{data.points,
                                     top_killer_kills,
                                     player_team_matrix_kills,
                                     hud_manager.get_registered_hud_count()});
    test_driver->advance_timeline();
}

void FTestHUDManagerScenario::player_kill_process_samples() {
    ml::check_samples_recorded(player_samples.num(), checks, TEXT("Player samples recorded"));
    if (player_samples.is_empty()) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        return;
    }

    auto const& before_sample{player_samples.nearest_value(early_sample_time)};
    auto const& sample{player_samples.nearest_value(test_duration)};
    checks.are_equal(0, before_sample.top_killer_kills, TEXT("Top killers start empty"));
    checks.are_equal(0, before_sample.player_team_matrix_kills, TEXT("Kill matrix starts empty"));
    checks.are_equal(1, sample.points, TEXT("Player kill count is cached"));
    checks.are_equal(1, sample.top_killer_kills, TEXT("Top-killer data is cached"));
    checks.are_equal(1, sample.player_team_matrix_kills, TEXT("Kill matrix is cached"));
    checks.are_equal(0, sample.registered_hud_count, TEXT("Player data updates without a HUD"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

/* ------------------------------------------------------------------------------------------ */
// Registration
/* ------------------------------------------------------------------------------------------ */
void FTestHUDManagerScenario::registration_process_samples() {
    if (!initialise_headless_hud_manager()) {
        return;
    }

    auto* const orchestrator{&context_.orchestrator};
    if (!checks.not_nullptr(orchestrator, TEXT("Orchestrator is available"))) {
        return;
    }
    auto& hud_manager{orchestrator->get_hud_manager()};
    checks.are_equal(orchestrator->get_entity_registry().get_num_alive_active_entities(),
                     count_cached_entities(hud_manager),
                     TEXT("Cache exists before HUD registration"));

    auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
    auto* const player_controller{context_.world.GetFirstPlayerController()};
    auto const ui_data_loaded{checks.not_nullptr(ui_data, TEXT("HUD UI data loads"))};
    auto const player_controller_available{
        checks.not_nullptr(player_controller, TEXT("Player controller is available"))};
    if (!ui_data_loaded || !player_controller_available) {
        return;
    }

    auto const widget_class{ui_data->get_widget_class<UShipHudWidget>()};
    if (!checks.is_true(static_cast<bool>(widget_class), TEXT("Project HUD class is configured"))) {
        return;
    }
    auto* const hud{
        CreateWidget<UShipHudWidget>(player_controller, widget_class, TEXT("test_hud"))};
    if (!checks.not_nullptr(hud, TEXT("Real project HUD is created"))) {
        return;
    }

    hud->set_crosshair_distances(ui_data->crosshair_distances);
    hud->AddToViewport();
    hud_manager.register_hud(*hud);
    checks.are_equal(1,
                     hud_manager.get_registered_hud_count(),
                     TEXT("Late HUD is registered and synchronised immediately"));

    hud_manager.unregister_hud(*hud);
    checks.are_equal(0, hud_manager.get_registered_hud_count(), TEXT("HUD unregisters cleanly"));
    hud->RemoveFromParent();

    hud_manager.set_selected_mapping_context(FString{TEXT("after_unregister")});
    hud_manager.tick(1);
    get_headless_hud_manager().set_selected_mapping_context(FString{TEXT("after_unregister")});
    tick_headless_hud_manager();
    checks.are_equal(0,
                     hud_manager.get_registered_hud_count(),
                     TEXT("Updates do not target the unregistered HUD"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

/* ------------------------------------------------------------------------------------------ */
// Polling scenario
/* ------------------------------------------------------------------------------------------ */
void FTestHUDManagerScenario::entity_count_pre_begin_play(UWorld& world,
                                                          USpaceGameLevelConfig const& config) {
    ml::spawn_capital_proxy(
        world, config, checks, FName{TEXT("entity_count_capital")}, FVector::ZeroVector);
}

void FTestHUDManagerScenario::entity_count_begin() {
    initialise_test_driver();
    if (!initialise_headless_hud_manager()) {
        return;
    }
    auto const& hud_manager{get_headless_hud_manager()};
    initial_alive_count = count_cached_entities(hud_manager);
    checks.are_equal(0,
                     hud_manager.get_registered_hud_count(),
                     TEXT("Entity cache starts with no registered HUD"));

    auto const& capitals{test_driver->get_capital_ships()};
    check(capitals.get_num_instances() == 1);
    TArray<FRegistryEntityHandle> const targets{capitals.get_handle(0)};
    test_driver->timeline.then_after(damage_queue_time,
                                     [this, targets] { test_driver->queue_kills(targets); });
    ml::reset_and_reserve_time_series(
        test_driver->orchestrator, test_duration, entity_count_samples);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTestHUDManagerScenario::entity_count_on_tick));
    test_driver->timeline.finish_at(test_duration);
}

void FTestHUDManagerScenario::entity_count_on_tick(ATestBatchOrchestrator& orchestrator) {
    tick_headless_hud_manager();
    entity_count_samples.add(
        test_driver->get_time(),
        FEntityCountSample{count_cached_entities(get_headless_hud_manager()),
                           orchestrator.get_hud_manager().get_registered_hud_count()});
    test_driver->advance_timeline();
}

void FTestHUDManagerScenario::entity_count_process_samples() {
    ml::check_samples_recorded(
        entity_count_samples.num(), checks, TEXT("Entity-count samples recorded"));
    if (entity_count_samples.is_empty()) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        return;
    }

    auto const& early_sample{entity_count_samples.nearest_value(early_sample_time)};
    auto const& final_sample{entity_count_samples.nearest_value(test_duration)};
    checks.are_equal(initial_alive_count,
                     early_sample.cached_alive_count,
                     TEXT("Entity cache waits for its polling period"));
    checks.are_equal(initial_alive_count - 1,
                     final_sample.cached_alive_count,
                     TEXT("Entity cache updates after the polling period"));
    checks.are_equal(
        0, final_sample.registered_hud_count, TEXT("Entity cache still has no registered HUD"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}
/* ------------------------------------------------------------------------------------------ */
// Shared
/* ------------------------------------------------------------------------------------------ */
auto FTestHUDManagerScenario::initialise_headless_hud_manager() -> bool {
    if (headless_hud_manager.IsSet()) {
        return true;
    }

    auto* const orchestrator{&context_.orchestrator};
    if (!checks.is_valid(orchestrator, TEXT("Orchestrator for headless HUD manager"))) {
        return false;
    }

    if (orchestrator->get_state() == EOrchestratorState::Uninitialised) {
        orchestrator->start_simulation();
    }

    auto& entity_registry{orchestrator->get_entity_registry()};

    headless_hud_manager.Emplace();
    headless_hud_manager->initialise(orchestrator->get_hud_update_frequencies(),
                                     orchestrator->get_mission_manager(),
                                     entity_registry,
                                     orchestrator->get_hud_tick_loop().tick_rate,
                                     orchestrator->get_player_ship_simulation(),
                                     *orchestrator->get_level_config(),
                                     {});
    return true;
}

auto FTestHUDManagerScenario::get_headless_hud_manager() -> FHUDManager& {
    check(headless_hud_manager.IsSet());
    return headless_hud_manager.GetValue();
}

void FTestHUDManagerScenario::tick_headless_hud_manager() {
    if (!headless_hud_manager.IsSet()) {
        checks.is_true(false, TEXT("Headless HUD manager is initialised before ticking"));
        return;
    }

    headless_hud_manager->tick(1);
}

void FTestHUDManagerScenario::check_headless_hud_manager_matches_simulation() {
    if (!headless_hud_manager.IsSet()) {
        return;
    }

    auto* const orchestrator{&context_.orchestrator};
    if (!checks.is_valid(orchestrator, TEXT("Orchestrator for HUD cache comparison"))) {
        return;
    }

    auto& simulation_hud_manager{orchestrator->get_hud_manager()};
    auto& local_hud_manager{get_headless_hud_manager()};
    local_hud_manager.force_sample();
    simulation_hud_manager.force_sample();
    check_mission_data_equal(local_hud_manager.get_mission_data(),
                             simulation_hud_manager.get_mission_data());
    checks.is_true(local_hud_manager.get_entity_count_data() ==
                       simulation_hud_manager.get_entity_count_data(),
                   TEXT("Headless and simulation entity-count caches match"));
    checks.is_true(local_hud_manager.get_kill_data() == simulation_hud_manager.get_kill_data(),
                   TEXT("Headless and simulation kill caches match"));
    checks.is_true(local_hud_manager.get_player_status_data() ==
                       simulation_hud_manager.get_player_status_data(),
                   TEXT("Headless and simulation player-status caches match"));
    checks.is_true(local_hud_manager.get_player_flight_data() ==
                       simulation_hud_manager.get_player_flight_data(),
                   TEXT("Headless and simulation player-flight caches match"));
#if WITH_EDITOR
    check_sampled_speed_data_equal(local_hud_manager.get_sampled_speed_data(),
                                   simulation_hud_manager.get_sampled_speed_data());
#endif
}

void FTestHUDManagerScenario::check_mission_data_equal(
    ml::hud_manager::FMissionDataCache const& local,
    ml::hud_manager::FMissionDataCache const& simulation) {
    auto const& local_static_data{local.static_data};
    auto const& simulation_static_data{simulation.static_data};
    checks.are_equal(local_static_data.mission_mode,
                     simulation_static_data.mission_mode,
                     TEXT("Headless and simulation mission modes match"));
    checks.is_true(local_static_data.surviving_entity_ids ==
                       simulation_static_data.surviving_entity_ids,
                   TEXT("Headless and simulation surviving mission entity IDs match"));
    checks.is_true(local_static_data.surviving_entity_types ==
                       simulation_static_data.surviving_entity_types,
                   TEXT("Headless and simulation surviving mission entity types match"));
    checks.is_true(local_static_data.required_kill_entity_ids ==
                       simulation_static_data.required_kill_entity_ids,
                   TEXT("Headless and simulation required-kill mission entity IDs match"));
    checks.is_true(local_static_data.required_kill_entity_types ==
                       simulation_static_data.required_kill_entity_types,
                   TEXT("Headless and simulation required-kill mission entity types match"));

    auto const& local_status_data{local.status_data};
    auto const& simulation_status_data{simulation.status_data};
    checks.are_equal(local_status_data.mission_state,
                     simulation_status_data.mission_state,
                     TEXT("Headless and simulation mission states match"));
    checks.are_equal(local_status_data.mission_stopwatch,
                     simulation_status_data.mission_stopwatch,
                     TEXT("Headless and simulation mission stopwatches match"));
    checks.are_equal(local_status_data.time_remaining,
                     simulation_status_data.time_remaining,
                     TEXT("Headless and simulation mission remaining times match"));
    checks.are_equal(local_status_data.enemies_remaining,
                     simulation_status_data.enemies_remaining,
                     TEXT("Headless and simulation mission enemy counts match"));

    auto const local_health_count{local_status_data.surviving_entity_health.Num()};
    auto const simulation_health_count{simulation_status_data.surviving_entity_health.Num()};
    checks.are_equal(local_health_count,
                     simulation_health_count,
                     TEXT("Headless and simulation surviving mission health counts match"));
    auto const health_count{FMath::Min(local_health_count, simulation_health_count)};
    for (int32 i{0}; i < health_count; ++i) {
        auto const& local_health{local_status_data.surviving_entity_health[i]};
        auto const& simulation_health{simulation_status_data.surviving_entity_health[i]};
        checks.are_equal(local_health.health,
                         simulation_health.health,
                         TEXT("Headless and simulation surviving mission health matches"),
                         i);
        checks.are_equal(local_health.max_health,
                         simulation_health.max_health,
                         TEXT("Headless and simulation surviving mission maximum health matches"),
                         i);
    }

    auto const local_required_health_count{local_status_data.required_kill_entity_health.Num()};
    auto const simulation_required_health_count{
        simulation_status_data.required_kill_entity_health.Num()};
    checks.are_equal(local_required_health_count,
                     simulation_required_health_count,
                     TEXT("Headless and simulation required-kill mission health counts match"));
    auto const required_health_count{
        FMath::Min(local_required_health_count, simulation_required_health_count)};
    for (int32 i{0}; i < required_health_count; ++i) {
        auto const& local_health{local_status_data.required_kill_entity_health[i]};
        auto const& simulation_health{simulation_status_data.required_kill_entity_health[i]};
        checks.are_equal(local_health.health,
                         simulation_health.health,
                         TEXT("Headless and simulation required-kill mission health matches"),
                         i);
        checks.are_equal(
            local_health.max_health,
            simulation_health.max_health,
            TEXT("Headless and simulation required-kill mission maximum health matches"),
            i);
    }
}

#if WITH_EDITOR
void FTestHUDManagerScenario::check_sampled_speed_data_equal(
    ml::hud_manager::FSampledSpeedDataCache const& local,
    ml::hud_manager::FSampledSpeedDataCache const& simulation) {
    constexpr double sampled_speed_tolerance{1e-6};
    auto const local_sample_count{local.samples.Num()};
    auto const simulation_sample_count{simulation.samples.Num()};
    checks.are_equal(local_sample_count,
                     simulation_sample_count,
                     TEXT("Headless and simulation sampled-speed cache lengths match"));
    checks.are_equal(local.oldest_index,
                     simulation.oldest_index,
                     TEXT("Headless and simulation sampled-speed cache indices match"));
    checks.all_equal(local.samples,
                     simulation.samples,
                     sampled_speed_tolerance,
                     TEXT("Headless and simulation sampled-speed samples match"));
}
#endif

auto FTestHUDManagerScenario::count_cached_entities(FHUDManager const& manager) -> int32 {
    int32 total{0};
    auto const& counts{manager.get_entity_count_data().alive_per_team_and_type};
    constexpr auto n_teams{ml::EnumCountTrait<ETestTeam>::count_value};
    constexpr auto n_types{ml::EnumCountTrait<ETestEntityType>::count_value};
    for (int32 team{0}; team < n_teams; ++team) {
        for (int32 type{0}; type < n_types; ++type) {
            total += counts[team][type];
        }
    }
    return total;
}

void FTestHUDManagerScenario::run() {
    switch (scenario_) {
        case EHUDManagerScenario::InitialCachesPopulateWithoutHUD:
            TestCommandBuilder.Do([this] { initial_caches_process_samples(); });
            break;
        case EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD:
            run_until_timeline_finished(
                [this] {
                    entity_count_pre_begin_play(context_.world, context_.config);
                    entity_count_begin();
                },
                timeout,
                [this] { entity_count_process_samples(); });
            break;
        case EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD:
            run_until_timeline_finished(
                [this] {
                    defence_pre_begin_play(context_.world, context_.config);
                    configure_defence_mission(context_.world, context_.orchestrator);
                    defence_begin();
                },
                timeout,
                [this] { defence_process_samples(); });
            break;
        case EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD:
            run_until_timeline_finished(
                [this] {
                    player_kill_pre_begin_play(context_.world, context_.config);
                    player_kill_begin();
                },
                timeout,
                [this] { player_kill_process_samples(); });
            break;
        case EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD:
            run_until_timeline_finished(
                [this] {
                    mission_time_pre_begin_play(context_.world, context_.config);
                    configure_defence_mission(context_.world, context_.orchestrator);
                    mission_time_begin();
                },
                timeout,
                [this] { mission_time_process_samples(); });
            break;
        case EHUDManagerScenario::LateHUDRegistrationSynchronisesAndUnregisters:
            TestCommandBuilder.Do([this] { registration_process_samples(); });
            break;
    }
}

}
