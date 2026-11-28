#include <SpaceGame/ships/player/PlayerModalUi.h>

#include <SpaceGame/levels/LevelCompletion.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ui/common/GameUiRootLayout.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGame/UnrealEndpoint.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/GameInstance.h>

namespace ioj {
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

    auto const root_class{ui_data->get_widget_class<UGameUiRootLayout>()};
    if (!root_class) {
        return false;
    }

    auto* const root{CreateWidget<UGameUiRootLayout>(&owner, root_class, TEXT("game_ui_root"))};
    if (!IsValid(root) || !root->initialise(*ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerModalUi::initialise_root: Failed to create root."));
        return false;
    }

    root_ = root;
    root->AddToPlayerScreen(100);
    return true;
}
void FPlayerModalUi::shutdown(ASpaceGamePlayerController& owner) {
    clear_menus(owner);
    if (IsValid(root_)) {
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
auto FPlayerModalUi::main_menu() const -> UMainMenuWidget* {
    return IsValid(root_) && !has_modal() ? root_->main_menu() : nullptr;
}
auto FPlayerModalUi::show_main_menu() -> bool {
    if (!has_root()) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerModalUi::show_main_menu: UI root is invalid."));
        return false;
    }

    return root_->show_main_menu();
}

/* **************************************** */
// Modal widgets and callbacks
/* **************************************** */
auto FPlayerModalUi::show_pause(ASpaceGamePlayerController& owner) -> bool {
    if (!has_root()) {
        UE_LOG(LogSandboxController, Error, TEXT("FPlayerModalUi: UI root is invalid."));
        return false;
    }

    pause_menu_ = root_->show_pause_menu();
    if (!IsValid(pause_menu_)) {
        return false;
    }

    pause_menu_->closed.RemoveAll(&owner);
    pause_menu_->closed.AddUObject(&owner, &ASpaceGamePlayerController::on_pause_menu_deactivated);
    return true;
}
auto FPlayerModalUi::show_completion(ASpaceGamePlayerController& owner,
                                     FLevelCompletion const& completion) -> bool {
    if (!has_root()) {
        UE_LOG(LogSandboxController, Error, TEXT("FPlayerModalUi: UI root is invalid."));
        return false;
    }

    completion_menu_ = root_->show_level_completion(completion);
    if (!IsValid(completion_menu_)) {
        return false;
    }

    completion_menu_->closed.RemoveAll(&owner);
    completion_menu_->closed.AddUObject(
        &owner, &ASpaceGamePlayerController::on_completion_menu_deactivated);
    return true;
}
void FPlayerModalUi::detach_callbacks(ASpaceGamePlayerController& owner) {
    if (IsValid(pause_menu_)) {
        pause_menu_->closed.RemoveAll(&owner);
    }

    if (IsValid(completion_menu_)) {
        completion_menu_->closed.RemoveAll(&owner);
    }
}
void FPlayerModalUi::clear_modals(ASpaceGamePlayerController& owner) {
    detach_callbacks(owner);
    if (IsValid(pause_menu_)) {
        pause_menu_->close();
    }

    if (IsValid(completion_menu_)) {
        completion_menu_->close();
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
        pause_menu_->closed.RemoveAll(&owner);
    }

    pause_menu_ = nullptr;
}
void FPlayerModalUi::close_completion(ASpaceGamePlayerController& owner) {
    if (IsValid(completion_menu_)) {
        completion_menu_->closed.RemoveAll(&owner);
    }

    completion_menu_ = nullptr;
}
auto FPlayerModalUi::has_modal() const -> bool {
    return IsValid(pause_menu_) || IsValid(completion_menu_);
}
auto FPlayerModalUi::has_completion() const -> bool {
    return IsValid(completion_menu_);
}
auto FPlayerModalUi::is_completion_active() const -> bool {
    return IsValid(completion_menu_) && completion_menu_->GetParent() != nullptr;
}

/* **************************************** */
// Gameplay restoration
/* **************************************** */
void FPlayerModalUi::begin_suspend(levels::ControlContext const context) {
    resume_.suspend(context);
}
void FPlayerModalUi::clear_resume() {
    resume_.clear();
}
void FPlayerModalUi::on_ship_changed(bool const has_ship) {
    resume_.on_player_changed(has_ship);
}
auto FPlayerModalUi::resume_context(levels::ControlContext const fallback) const
    -> levels::ControlContext {
    return resume_.context(fallback);
}

} // namespace ioj

namespace ioj {
auto FPlayerModalUi::completed_level_id() const -> FName {
    return IsValid(completion_menu_) ? completion_menu_->completion().level_id : NAME_None;
}
}
