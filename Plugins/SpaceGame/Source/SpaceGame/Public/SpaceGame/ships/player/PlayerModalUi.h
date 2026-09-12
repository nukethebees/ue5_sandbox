#pragma once

#include <CoreMinimal.h>
#include <SpaceGame/ships/player/PlayerControlContext.h>
#include <SpaceGameSimulation/simulation/LevelTelemetrySnapshot.h>
#include "PlayerModalUi.generated.h"

class ASpaceGamePlayerController;
class UTestBatchGameUiData;
class UInputAction;
struct FTestMissionCompletion;
namespace ml::ioj {
class UGameUiRootLayout;
class UPauseMenuWidget;
class ULevelCompletionWidget;
struct FPauseMenuData;
}

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
    auto show_pause(ASpaceGamePlayerController& owner,
                    UInputAction& action,
                    ml::ioj::FPauseMenuData data) -> bool;
    auto show_completion(ASpaceGamePlayerController& owner,
                         FTestMissionCompletion const& completion,
                         FLevelTelemetrySnapshot snapshot) -> bool;
    void detach_callbacks(ASpaceGamePlayerController& owner);
    void clear_modals(ASpaceGamePlayerController& owner);
    void clear_menus(ASpaceGamePlayerController& owner);
    void close_pause(ASpaceGamePlayerController& owner);
    void close_completion(ASpaceGamePlayerController& owner);
    void deactivate_pause();
    auto has_root() const -> bool;
    auto has_modal() const -> bool;
    auto has_completion() const -> bool;
    auto is_pause_active() const -> bool;
    auto is_completion_active() const -> bool;

    /* **************************************** */
    // Gameplay restoration
    /* **************************************** */
    void begin_suspend(EPlayerControlContext context);
    void clear_resume();
    void on_ship_changed(bool has_ship);
    auto resume_context(EPlayerControlContext fallback) const -> EPlayerControlContext;
    auto is_resume_pending() const -> bool { return resume_pending_; }
  private:
    friend struct FPlayerControllerTestAccess;
    UPROPERTY(Transient)
    TObjectPtr<ml::ioj::UGameUiRootLayout> root_{nullptr};
    UPROPERTY(Transient)
    TObjectPtr<ml::ioj::UPauseMenuWidget> pause_menu_{nullptr};
    UPROPERTY(Transient)
    TObjectPtr<ml::ioj::ULevelCompletionWidget> completion_menu_{nullptr};
    EPlayerControlContext restore_context_{EPlayerControlContext::None};
    bool resume_pending_{false};
};
