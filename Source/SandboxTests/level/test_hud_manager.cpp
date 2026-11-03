#include "test_hud_manager_scenario.h"
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>
#include <SpaceGamePresentation/presentation/widgets/ShipHudWidget.h>

#include <Components/TextBlock.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>

namespace ioj {
namespace {
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

FTestHUDManagerScenario::FTestHUDManagerScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

/* ------------------------------------------------------------------------------------------ */
// Registration
/* ------------------------------------------------------------------------------------------ */
void FTestHUDManagerScenario::registration_process_samples() {
    auto* const player{spawn_player_ship(
        context_.world, context_.config.classes.player_ship_class, &context_.config.player_ship)};
    if (!checks.is_valid(player, TEXT("Player exists for overlay and radar collection"))) {
        return;
    }
    context_.orchestrator.set_player_ship(*player);
    context_.orchestrator.start_simulation();
    auto* const orchestrator{&context_.orchestrator};
    auto const* const level{orchestrator->get_level_simulation()};
    check(level);
    FHUDManager hud_manager;
    hud_manager.configure(orchestrator->get_hud_manager().settings());
    hud_manager.initialise(level->mission(),
                           level->read_access(),
                           level->spatial_query_manager(),
                           context_.config.get_visual_config());
    checks.are_equal(level->entity_ledger().count_alive(),
                     count_cached_hud_entities(hud_manager),
                     TEXT("Cache exists before HUD registration"));
    hud_manager.set_selected_mapping_context(TEXT("late_registration"));
    hud_manager.tick(1);

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

    auto* const imc{Cast<UUserWidget>(hud->GetWidgetFromName(TEXT("selected_imc_widget")))};
    auto* const text{imc ? Cast<UTextBlock>(imc->GetWidgetFromName(TEXT("value_text"))) : nullptr};
    if (checks.not_nullptr(text, TEXT("Selected mapping context text exists"))) {
        checks.is_true(text->GetText().ToString().Contains(TEXT("late_registration")),
                       TEXT("Late HUD registration immediately synchronises the cached model"));
    }

    hud_manager.set_selected_mapping_context(TEXT("registered_update"));
    hud_manager.tick(1);
    if (IsValid(text)) {
        checks.is_true(text->GetText().ToString().Contains(TEXT("registered_update")),
                       TEXT("Registered ship HUD receives flight updates"));
    }

    hud_manager.unregister_hud(*hud);
    checks.are_equal(0, hud_manager.get_registered_hud_count(), TEXT("HUD unregisters cleanly"));
    hud_manager.register_hud(*hud);
    checks.are_equal(
        1, hud_manager.get_registered_hud_count(), TEXT("HUD can register again without a tick"));
    hud_manager.unregister_hud(*hud);
    hud->RemoveFromParent();

    hud_manager.set_selected_mapping_context(FString{TEXT("after_unregister")});
    hud_manager.tick(1);
    if (IsValid(text)) {
        checks.is_true(text->GetText().ToString().Contains(TEXT("registered_update")),
                       TEXT("Unregistered widget retains its last published model"));
    }
    checks.are_equal(0,
                     hud_manager.get_registered_hud_count(),
                     TEXT("Updates do not target the unregistered HUD"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FTestHUDManagerScenario::run() {
    TestCommandBuilder.Do([this] { registration_process_samples(); });
}

}
