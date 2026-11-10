#pragma once

#include <ioj/levels/control_context.h>
#include <ioj/session/modal_resume_state.h>

#include <CoreMinimal.h>

#include "PlayerModalUi.generated.h"

class UInputAction;
struct FLevelCompletion;
struct FPlayerControllerTestAccess;

namespace ioj {
class UTestBatchGameUiData;
class ASpaceGamePlayerController;
class UGameUiRootLayout;
class UPauseMenuWidget;
class ULevelCompletionWidget;

USTRUCT()
struct SPACEGAME_API FPlayerModalUi {
    GENERATED_BODY()

    /* **************************************** */
    // Root lifecycle
    /* **************************************** */
    auto initialise_root(ASpaceGamePlayerController& owner, UTestBatchGameUiData* ui_data) -> bool;
    void shutdown(ASpaceGamePlayerController& owner);
    /* **************************************** */
    // Main menu
    /* **************************************** */
    auto show_main_menu(ASpaceGamePlayerController& owner) -> bool;
    void apply_main_menu_input_mode(ASpaceGamePlayerController& owner);
    /* **************************************** */
    // Modal widgets and callbacks
    /* **************************************** */
    auto show_pause(ASpaceGamePlayerController& owner, UInputAction& action) -> bool;
    auto show_completion(ASpaceGamePlayerController& owner, FLevelCompletion const& completion)
        -> bool;
    void detach_callbacks(ASpaceGamePlayerController& owner);
    void clear_modals(ASpaceGamePlayerController& owner);
    void clear_menus(ASpaceGamePlayerController& owner);
    void close_pause(ASpaceGamePlayerController& owner);
    void close_completion(ASpaceGamePlayerController& owner);
    void deactivate_pause();
    auto has_root() const -> bool;
    auto has_modal() const -> bool;
    auto completed_level_id() const -> FName;
    auto has_completion() const -> bool;
    auto is_pause_active() const -> bool;
    auto is_completion_active() const -> bool;

    /* **************************************** */
    // Gameplay restoration
    /* **************************************** */
    void begin_suspend(levels::ControlContext context);
    void clear_resume();
    void on_ship_changed(bool has_ship);
    auto resume_context(levels::ControlContext fallback) const -> levels::ControlContext;
    auto is_resume_pending() const -> bool { return resume_.pending(); }
  private:
    friend struct ::FPlayerControllerTestAccess;
    UPROPERTY(Transient)
    TObjectPtr<UGameUiRootLayout> root_{nullptr};
    UPROPERTY(Transient)
    TObjectPtr<UPauseMenuWidget> pause_menu_{nullptr};
    UPROPERTY(Transient)
    TObjectPtr<ULevelCompletionWidget> completion_menu_{nullptr};
    session::ModalResumeState resume_;
};

} // namespace ioj
