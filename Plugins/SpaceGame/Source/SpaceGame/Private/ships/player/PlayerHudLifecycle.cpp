#include <SpaceGame/ships/player/PlayerHudLifecycle.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <Engine/GameInstance.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGamePresentation/presentation/widgets/BattleViewerHudWidget.h>
#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>
#include <SpaceGamePresentation/presentation/widgets/ShipHudWidget.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

auto FPlayerHudLifecycle::initialise(ASpaceGamePlayerController& owner,
                                     ATestBatchOrchestrator* orchestrator,
                                     UTestBatchGameUiData* ui_data,
                                     EPlayerControlContext const context,
                                     float const observer_speed) -> bool {
    if (IsValid(hud_widget_)) {
        auto const correct_type{context == EPlayerControlContext::Player
                                    ? hud_widget_->IsA<UShipHudWidget>()
                                    : context == EPlayerControlContext::Observer &&
                                          hud_widget_->IsA<UBattleViewerHudWidget>()};
        if (correct_type) {
            return true;
        }
        shutdown();
    }

    auto* const world{owner.GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("FPlayerHudLifecycle::initialise: World is not available yet."));
        return false;
    }
    if (!IsValid(owner.GetLocalPlayer())) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("FPlayerHudLifecycle::initialise: Local player is not available "
                    "yet."));
        return false;
    }

    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("FPlayerHudLifecycle::initialise: Orchestrator is not available "
                    "yet."));
        return false;
    }
    if (!IsValid(ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerHudLifecycle::initialise: UI data is invalid."));
        return false;
    }

    auto* const team_visual_data{ui_data->team_visual_data.Get()};
    if (!IsValid(team_visual_data)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("FPlayerHudLifecycle::initialise: Team visual data is invalid."));
        return false;
    }

    TSubclassOf<USimulationHudWidget> hud_widget_class;
    FName widget_name;
    if (context == EPlayerControlContext::Player) {
        hud_widget_class = ui_data->get_widget_class<UShipHudWidget>();
        widget_name = TEXT("ship_hud");
    } else if (context == EPlayerControlContext::Observer) {
        hud_widget_class = ui_data->get_widget_class<UBattleViewerHudWidget>();
        widget_name = TEXT("battle_viewer_hud");
    } else {
        return false;
    }
    if (!hud_widget_class) {
        return false;
    }

    auto* const created_widget{
        CreateWidget<USimulationHudWidget>(&owner, hud_widget_class, widget_name)};
    if (!IsValid(created_widget)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("FPlayerHudLifecycle::initialise: Failed to create HUD widget."));
        return false;
    }

    hud_widget_ = created_widget;
    auto* const game_subsystem{owner.GetGameInstance()->GetSubsystem<ml::ioj::UGameSubsystem>()};
    if (IsValid(game_subsystem)) {
        created_widget->apply_ui_style(game_subsystem->get_ui_style());
    } else {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("FPlayerHudLifecycle::initialise: Game subsystem is invalid."));
    }
    created_widget->AddToViewport();
    created_widget->set_entity_colours(team_visual_data->build_team_colour_cache());
    if (auto* const ship_hud{Cast<UShipHudWidget>(created_widget)}; IsValid(ship_hud)) {
        ship_hud->set_crosshair_distances(ui_data->crosshair_distances);
    }
    if (auto* const viewer_hud{Cast<UBattleViewerHudWidget>(created_widget)}; IsValid(viewer_hud)) {
        viewer_hud->set_movement_speed(observer_speed);
    }
    orchestrator->get_hud_manager().register_hud(*created_widget);
    registered_orchestrator_ = orchestrator;
    return true;
}
void FPlayerHudLifecycle::shutdown() {
    if (!IsValid(hud_widget_)) {
        hud_widget_ = nullptr;
        registered_orchestrator_.Reset();
        restore_pending_ = false;
        return;
    }
    if (auto* const orchestrator{registered_orchestrator_.Get()};
        IsValid(orchestrator) && orchestrator->get_hud_manager().get_registered_hud_count() > 0) {
        orchestrator->get_hud_manager().unregister_hud(*hud_widget_);
    }
    hud_widget_->RemoveFromParent();
    hud_widget_ = nullptr;
    registered_orchestrator_.Reset();
    restore_pending_ = false;
}
auto FPlayerHudLifecycle::initialise_benchmark(ASpaceGamePlayerController& owner,
                                               ATestBatchOrchestrator* orchestrator,
                                               UTestBatchGameUiData* ui_data) -> bool {
    if (IsValid(benchmark_hud_widget_)) {
        return true;
    }
    auto* const world{owner.GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("FPlayerHudLifecycle::initialise_benchmark: World is not "
                    "available yet."));
        return false;
    }
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("FPlayerHudLifecycle::initialise_benchmark: Orchestrator is not "
                    "available yet."));
        return false;
    }
    if (!IsValid(ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerHudLifecycle::initialise_benchmark: UI data is invalid."));
        return false;
    }

    auto const widget_class{ui_data->get_widget_class<UBenchmarkHudWidget>()};
    if (!widget_class) {
        return false;
    }
    auto* const created_widget{
        CreateWidget<UBenchmarkHudWidget>(&owner, widget_class, TEXT("benchmark_hud"))};
    if (!IsValid(created_widget)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerHudLifecycle::initialise_benchmark: Failed to create "
                    "widget."));
        return false;
    }

    benchmark_hud_widget_ = created_widget;
    if (auto* const game_subsystem{
            owner.GetGameInstance()->GetSubsystem<ml::ioj::UGameSubsystem>()};
        IsValid(game_subsystem)) {
        created_widget->apply_ui_style(game_subsystem->get_ui_style());
    }
    created_widget->set_tick_source([weak = TWeakObjectPtr<ATestBatchOrchestrator>{orchestrator}] {
        return weak.IsValid() ? weak->get_benchmark_ticks_remaining() : TOptional<uint64>{};
    });
    created_widget->end_requested.AddUObject(&owner,
                                             &ASpaceGamePlayerController::return_to_level_select);
    created_widget->AddToPlayerScreen(100);
    created_widget->ActivateWidget();
    return true;
}
void FPlayerHudLifecycle::shutdown_benchmark(ASpaceGamePlayerController& owner) {
    if (!IsValid(benchmark_hud_widget_)) {
        return;
    }
    benchmark_hud_widget_->end_requested.RemoveAll(&owner);
    benchmark_hud_widget_->DeactivateWidget();
    benchmark_hud_widget_->RemoveFromParent();
    benchmark_hud_widget_ = nullptr;
}
void FPlayerHudLifecycle::hide_for_modal() {
    if (restore_pending_ || !IsValid(hud_widget_)) {
        return;
    }

    visibility_before_modal_ = hud_widget_->GetVisibility();
    restore_pending_ = true;
    hud_widget_->SetVisibility(ESlateVisibility::Collapsed);
}
void FPlayerHudLifecycle::restore_after_modal() {
    if (!restore_pending_) {
        return;
    }

    if (IsValid(hud_widget_)) {
        hud_widget_->SetVisibility(visibility_before_modal_);
    }
    restore_pending_ = false;
}
void FPlayerHudLifecycle::set_observer_speed(float const speed) {
    if (auto* const viewer_hud{Cast<UBattleViewerHudWidget>(hud_widget_)}; IsValid(viewer_hud)) {
        viewer_hud->set_movement_speed(speed);
    }
}
void FPlayerHudLifecycle::cancel_restore() {
    restore_pending_ = false;
}
