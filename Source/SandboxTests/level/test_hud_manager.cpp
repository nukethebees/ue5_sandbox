#include "test_hud_manager_scenario.h"
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/missions/compiled_mission.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>
#include <SpaceGamePresentation/presentation/widgets/ShipHudWidget.h>

#include <SandboxCore/container_ops.h>

#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <Misc/Optional.h>

#include <array>

namespace ioj {
namespace {
auto defence_objectives() -> levels::MissionDefinition {
    return {
        .objectives = {{.id = ioj::levels::ObjectiveName{"defence"},
                        .title = "Hold position",
                        .team = Team::Blue,
                        .expressions = {{ioj::levels::TimeElapsedExpr{
                                            .seconds = ioj::NonNegativeFinite<double>::make(0.2)}},
                                        {ioj::levels::EntityDestroyedExpr{
                                            .entity = levels::EntityId{"defended"}}}},
                        .roots = ioj::levels::ObjectiveRoots{std::array<ioj::levels::ExprIndex, 3>{
                            0, 1, levels::absent_expression}}}}};
}
auto count_cached_hud_entities(ioj::FHUDManager const& manager) -> int32 {
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
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const needs_defence{
        scenario == EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD ||
        scenario == EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD};
    auto const needs_player{scenario == EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD};
    if (needs_player) {
        add_worldless_player_spawn(data, make_worldless_player_spawn(config));
    }
    uint32 first_capital_index{sim::invalid_level_entity_index};
    uint32 second_capital_index{sim::invalid_level_entity_index};
    if (scenario == EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD || needs_player) {
        first_capital_index = add_worldless_capital_spawn(data,
                                                          FVector3f{2000.f, 0.f, 0.f},
                                                          ETestTeam::Red,
                                                          sim::invalid_level_entity_index,
                                                          60.f,
                                                          60.f);
    } else if (needs_defence) {
        first_capital_index = add_worldless_capital_spawn(data,
                                                          FVector3f::ZeroVector,
                                                          ETestTeam::Blue,
                                                          sim::invalid_level_entity_index,
                                                          60.f,
                                                          60.f);
        second_capital_index = add_worldless_capital_spawn(data,
                                                           FVector3f{2000.f, 0.f, 0.f},
                                                           ETestTeam::Red,
                                                           sim::invalid_level_entity_index,
                                                           60.f,
                                                           60.f);
    }

    if (needs_defence) {
        std::array const ids{levels::EntityId{"defended"}, levels::EntityId{"enemy"}};
        sim::SimClock clock;
        clock.initialise(data.clock_settings);
        auto compiled{sim::CompiledMission::compile(defence_objectives(), ids, clock)};
        check(compiled);
        data.mission = std::move(*compiled);
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    auto& simulation{harness.get_simulation()};
    auto& mission{simulation.get_mission()};
    auto const& capitals{simulation.get_capital_ships()};
    auto const first_capital{first_capital_index == sim::invalid_level_entity_index
                                 ? sim::EntityId{}
                                 : capitals.get_id(first_capital_index)};
    auto const second_capital{second_capital_index == sim::invalid_level_entity_index
                                  ? sim::EntityId{}
                                  : capitals.get_id(second_capital_index)};
    harness.finish_initialisation();

    ioj::FHUDManager hud;
    hud.initialise(mission,
                   harness.get_ledger(),
                   simulation.get_capital_ships(),
                   simulation.get_fighters(),
                   simulation.get_turrets(),
                   simulation.get_spinners(),
                   simulation.get_spatial_query_manager(),
                   simulation.get_player_ship_simulation(),
                   config.get_visual_config(),
                   nullptr);
    checks.are_equal(0, hud.get_registered_hud_count(), TEXT("No HUD widgets are registered"));
    checks.are_equal(ioj::EHUDManagerState::Active, hud.get_state(), TEXT("HUD manager is active"));
    checks.are_equal(harness.get_ledger().count_alive(),
                     count_cached_hud_entities(hud),
                     TEXT("Initial entity cache matches ledger"));
    checks.are_equal(static_cast<int32>(mission.active_objectives().size()),
                     hud.get_mission_data().active,
                     TEXT("Active objective count is cached"));
    if (scenario == EHUDManagerScenario::InitialCachesPopulateWithoutHUD) {
        return;
    }

    auto const initial_count{count_cached_hud_entities(hud)};
    if (scenario == EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD) {
        harness.timeline.then_after(0.1, [&] { harness.queue_kills(std::array{first_capital}); });
    } else if (scenario == EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD) {
        harness.timeline.then_after(0.1, [&] {
            harness.queue_kills(std::array{second_capital});
            harness.queue_kills(std::array{first_capital});
        });
    } else if (needs_player) {
        auto const player_id{simulation.get_player_ship_simulation()->unique_entity_id};
        harness.timeline.then_after(
            0.1, [&] { harness.queue_kills(std::array{first_capital}, player_id); });
    }
    harness.on_end_tick = [&](sim::LevelSim&) { hud.force_sample(); };
    harness.timeline.finish_at(0.35);
    test.TestTrue(TEXT("HUD cache timeline completes"), harness.run_until_timeline_finished(1.0));

    if (scenario == EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD) {
        checks.are_equal(initial_count - 1,
                         count_cached_hud_entities(hud),
                         TEXT("Entity cache updates without a HUD"));
    } else if (scenario == EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD) {
        checks.are_equal(1, hud.get_mission_data().completed, TEXT("Resolved objective is cached"));
        checks.are_equal(
            0, hud.get_mission_data().active, TEXT("Resolved objective is no longer active"));
        checks.is_true(mission.outcome(ioj::sim::ObjectiveId{0}) == ObjectiveOutcome::Failed,
                       TEXT("Defence objective fails"));
    } else if (needs_player) {
        auto const& player_status{hud.get_player_status_data()};
        checks.is_true(player_status.has_player_ship, TEXT("Player HUD state is available"));
        checks.are_equal(1, player_status.points, TEXT("Player kill count is cached"));
        checks.are_equal(1,
                         harness.get_ledger().count_kills(),
                         TEXT("Kill data source records the player kill"));
    } else {
        checks.are_equal(1, hud.get_mission_data().completed, TEXT("Timer objective resolves"));
        checks.is_true(mission.outcome(ioj::sim::ObjectiveId{0}) == ObjectiveOutcome::Passed,
                       TEXT("Timer objective passes"));
    }
}

FTestHUDManagerScenario::FTestHUDManagerScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

void FTestHUDManagerScenario::on_tear_down() {
    check_headless_hud_manager_matches_simulation();
    reset(checks, test_driver, headless_hud_manager);
}

/* ------------------------------------------------------------------------------------------ */
// Registration
/* ------------------------------------------------------------------------------------------ */
void FTestHUDManagerScenario::registration_process_samples() {
    auto* const player{spawn_player_ship(
        context_.world, context_.config.classes.player_ship_class, &context_.config.player_ship)};
    if (!checks.is_valid(player, TEXT("Player exists for overlay and radar collection"))) {
        return;
    }
    initialise_test_driver();
    context_.orchestrator.set_player_ship(*player);
    context_.orchestrator.start_simulation();
    if (!initialise_headless_hud_manager()) {
        return;
    }

    auto* const orchestrator{&context_.orchestrator};
    if (!checks.not_nullptr(orchestrator, TEXT("Orchestrator is available"))) {
        return;
    }
    // This manager has never had a widget or an overlay tick. The orchestrator's manager
    // may already have been sampled, which masked the late-registration startup bug.
    auto& hud_manager{get_headless_hud_manager()};
    checks.are_equal(orchestrator->get_entity_ledger().count_alive(),
                     count_cached_hud_entities(hud_manager),
                     TEXT("Cache exists before HUD registration"));

    auto* const ui_data{test_batch_game_ui_data::get_data_asset()};
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
    checks.are_equal(ESlateVisibility::HitTestInvisible,
                     hud->GetVisibility(),
                     TEXT("Ship HUD does not intercept player input"));
    hud_manager.register_hud(*hud);
    checks.are_equal(1,
                     hud_manager.get_registered_hud_count(),
                     TEXT("Late HUD is registered and synchronised immediately"));

    hud_manager.unregister_hud(*hud);
    checks.are_equal(0, hud_manager.get_registered_hud_count(), TEXT("HUD unregisters cleanly"));
    hud_manager.register_hud(*hud);
    checks.are_equal(
        1, hud_manager.get_registered_hud_count(), TEXT("HUD can register again without a tick"));
    hud_manager.unregister_hud(*hud);
    hud->RemoveFromParent();

    hud_manager.set_selected_mapping_context(FString{TEXT("after_unregister")});
    orchestrator->get_hud_manager().set_selected_mapping_context(FString{TEXT("after_unregister")});
    orchestrator->get_hud_manager().tick(1);
    tick_headless_hud_manager();
    checks.are_equal(0,
                     hud_manager.get_registered_hud_count(),
                     TEXT("Updates do not target the unregistered HUD"));

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

    if (orchestrator->get_state() == sim::OrchestratorState::Uninitialised) {
        orchestrator->start_simulation();
    }

    auto const& entity_ledger{orchestrator->get_entity_ledger()};

    headless_hud_manager.Emplace();
    headless_hud_manager->configure(orchestrator->get_hud_manager().settings());
    headless_hud_manager->initialise(orchestrator->get_mission(),
                                     entity_ledger,
                                     orchestrator->get_level_simulation()->get_capital_ships(),
                                     orchestrator->get_level_simulation()->get_fighters(),
                                     orchestrator->get_level_simulation()->get_turrets(),
                                     orchestrator->get_level_simulation()->get_spinners(),
                                     orchestrator->get_spatial_query_manager(),
                                     orchestrator->get_player_ship_simulation(),
                                     orchestrator->get_level_config()->get_visual_config(),
                                     nullptr);
    return true;
}

auto FTestHUDManagerScenario::get_headless_hud_manager() -> ioj::FHUDManager& {
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
    checks.is_true(local_hud_manager.get_mission_data() ==
                       simulation_hud_manager.get_mission_data(),
                   TEXT("Objective caches match"));
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

#if WITH_EDITOR
void FTestHUDManagerScenario::check_sampled_speed_data_equal(
    hud_manager::FSampledSpeedDataCache const& local,
    hud_manager::FSampledSpeedDataCache const& simulation) {
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

void FTestHUDManagerScenario::run() {
    TestCommandBuilder.Do([this] { registration_process_samples(); });
}

}
