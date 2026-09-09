#include <SpaceGame/ships/player/PlayerModalUi.h>

#include <Engine/GameInstance.h>
#include <InputAction.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/ui/common/GameUiRootLayout.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGameSimulation/missions/TestMissionManager.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

/* **************************************** */
// Root lifecycle
/* **************************************** */
auto FPlayerModalUi::initialise_root(ASpaceGamePlayerController& owner,
                                     UTestBatchGameUiData* ui_data) -> bool {
    if (IsValid(root_)) {
        return true;
    }
    if (!IsValid(ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerModalUi::initialise_root: UI data is invalid."));
        return false;
    }

    auto const root_class{ui_data->get_widget_class<ml::ioj::UGameUiRootLayout>()};
    if (!root_class) {
        return false;
    }
    auto* const root{
        CreateWidget<ml::ioj::UGameUiRootLayout>(&owner, root_class, TEXT("game_ui_root"))};
    if (!IsValid(root) || !root->initialise(*ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerModalUi::initialise_root: Failed to create root."));
        return false;
    }

    root_ = root;
    root->AddToPlayerScreen(100);
    root->ActivateWidget();
    return true;
}
void FPlayerModalUi::shutdown(ASpaceGamePlayerController& owner) {
    clear_menus(owner);
    if (IsValid(root_)) {
        root_->DeactivateWidget();
        root_->RemoveFromParent();
    }
    root_ = nullptr;
}
auto FPlayerModalUi::has_root() const -> bool {
    return IsValid(root_);
}

/* **************************************** */
// Main menu
/* **************************************** */
auto FPlayerModalUi::show_main_menu(ASpaceGamePlayerController& owner) -> bool {
    if (!has_root()) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerModalUi::show_main_menu: UI root is invalid."));
        return false;
    }
    auto* const game_instance{owner.GetGameInstance()};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    auto level_select_request{IsValid(subsystem) ? subsystem->take_level_select_request()
                                                 : TOptional<ml::ioj::FLevelSelectRequest>{}};
    auto const show_level_select{IsValid(subsystem) && (level_select_request.IsSet() ||
                                                        subsystem->has_level_launch_error())};
    auto const preferred_level_id{
        level_select_request.IsSet() ? level_select_request->preferred_level_id : NAME_None};
    auto const show_telemetry{level_select_request.IsSet() &&
                              level_select_request->destination ==
                                  ml::ioj::EMainMenuDestination::Telemetry};
    auto const telemetry_run_id{
        level_select_request.IsSet() ? level_select_request->selected_telemetry_run_id : FString{}};
    auto const telemetry_error{level_select_request.IsSet() ? level_select_request->telemetry_error
                                                            : FString{}};
    if (!root_->show_main_menu(show_level_select,
                               preferred_level_id,
                               show_telemetry,
                               telemetry_run_id,
                               telemetry_error)) {
        return false;
    }

    return true;
}
void FPlayerModalUi::apply_main_menu_input_mode(ASpaceGamePlayerController& owner) {
    if (!IsValid(root_) || !IsValid(root_->get_active_screen())) {
        return;
    }

    FInputModeUIOnly input_mode{};
    input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    owner.SetInputMode(input_mode);
    owner.SetShowMouseCursor(true);
}

/* **************************************** */
// Modal widgets and callbacks
/* **************************************** */
auto FPlayerModalUi::show_pause(ASpaceGamePlayerController& owner,
                                UInputAction& action,
                                ml::ioj::FPauseMenuData data) -> bool {
    if (!has_root()) {
        UE_LOG(LogSandboxController, Error, TEXT("FPlayerModalUi: UI root is invalid."));
        return false;
    }
    pause_menu_ = root_->show_pause_menu(action, MoveTemp(data));
    if (!IsValid(pause_menu_)) {
        return false;
    }

    pause_menu_->return_to_level_select_requested.RemoveAll(&owner);
    pause_menu_->return_to_level_select_requested.AddUObject(
        &owner, &ASpaceGamePlayerController::return_to_level_select);
    pause_menu_->quit_requested.RemoveAll(&owner);
    pause_menu_->quit_requested.AddUObject(&owner, &ASpaceGamePlayerController::quit_game);
    pause_menu_->OnDeactivated().RemoveAll(&owner);
    pause_menu_->OnDeactivated().AddUObject(&owner,
                                            &ASpaceGamePlayerController::on_pause_menu_deactivated);
    return true;
}
auto FPlayerModalUi::show_completion(ASpaceGamePlayerController& owner,
                                     FTestMissionCompletion const& completion,
                                     FLevelTelemetrySnapshot snapshot) -> bool {
    if (!has_root()) {
        UE_LOG(LogSandboxController, Error, TEXT("FPlayerModalUi: UI root is invalid."));
        return false;
    }
    completion_menu_ = root_->show_level_completion(
        completion.level_display_name, completion.state, MoveTemp(snapshot));
    if (!IsValid(completion_menu_)) {
        return false;
    }
    completion_menu_->return_to_level_select_requested.RemoveAll(&owner);
    completion_menu_->return_to_level_select_requested.AddUObject(
        &owner, &ASpaceGamePlayerController::return_to_level_select);
    completion_menu_->OnDeactivated().RemoveAll(&owner);
    completion_menu_->OnDeactivated().AddUObject(
        &owner, &ASpaceGamePlayerController::on_completion_menu_deactivated);
    return true;
}
void FPlayerModalUi::detach_callbacks(ASpaceGamePlayerController& owner) {
    if (IsValid(pause_menu_)) {
        pause_menu_->OnDeactivated().RemoveAll(&owner);
        pause_menu_->return_to_level_select_requested.RemoveAll(&owner);
        pause_menu_->quit_requested.RemoveAll(&owner);
    }
    if (IsValid(completion_menu_)) {
        completion_menu_->OnDeactivated().RemoveAll(&owner);
        completion_menu_->return_to_level_select_requested.RemoveAll(&owner);
    }
}
void FPlayerModalUi::clear_modals(ASpaceGamePlayerController& owner) {
    detach_callbacks(owner);
    if (IsValid(pause_menu_)) {
        pause_menu_->DeactivateWidget();
    }
    if (IsValid(completion_menu_)) {
        completion_menu_->DeactivateWidget();
    }
    pause_menu_ = nullptr;
    completion_menu_ = nullptr;
    clear_resume();
}
void FPlayerModalUi::clear_menus(ASpaceGamePlayerController& owner) {
    clear_modals(owner);
    if (IsValid(root_)) {
        root_->clear_menus();
    }
}
void FPlayerModalUi::close_pause(ASpaceGamePlayerController& owner) {
    if (IsValid(pause_menu_)) {
        pause_menu_->OnDeactivated().RemoveAll(&owner);
        pause_menu_->return_to_level_select_requested.RemoveAll(&owner);
        pause_menu_->quit_requested.RemoveAll(&owner);
    }
    pause_menu_ = nullptr;
}
void FPlayerModalUi::close_completion(ASpaceGamePlayerController& owner) {
    if (IsValid(completion_menu_)) {
        completion_menu_->OnDeactivated().RemoveAll(&owner);
        completion_menu_->return_to_level_select_requested.RemoveAll(&owner);
    }
    completion_menu_ = nullptr;
}
void FPlayerModalUi::deactivate_pause() {
    if (IsValid(pause_menu_)) {
        pause_menu_->DeactivateWidget();
    }
}
auto FPlayerModalUi::has_modal() const -> bool {
    return IsValid(pause_menu_) || IsValid(completion_menu_);
}
auto FPlayerModalUi::has_completion() const -> bool {
    return IsValid(completion_menu_);
}
auto FPlayerModalUi::is_pause_active() const -> bool {
    return IsValid(pause_menu_) && pause_menu_->IsActivated();
}
auto FPlayerModalUi::is_completion_active() const -> bool {
    return IsValid(completion_menu_) && completion_menu_->IsActivated();
}

/* **************************************** */
// Gameplay restoration
/* **************************************** */
void FPlayerModalUi::begin_suspend(EPlayerControlContext const context) {
    restore_context_ = context;
    resume_pending_ = true;
}
void FPlayerModalUi::clear_resume() {
    restore_context_ = EPlayerControlContext::None;
    resume_pending_ = false;
}
void FPlayerModalUi::on_ship_changed(bool const has_ship) {
    if (!resume_pending_) {
        return;
    }
    if (has_ship) {
        restore_context_ = EPlayerControlContext::Player;
    } else if (restore_context_ == EPlayerControlContext::Player) {
        restore_context_ = EPlayerControlContext::None;
    }
}
auto FPlayerModalUi::resume_context(EPlayerControlContext const fallback) const
    -> EPlayerControlContext {
    return resume_pending_ ? restore_context_ : fallback;
}
