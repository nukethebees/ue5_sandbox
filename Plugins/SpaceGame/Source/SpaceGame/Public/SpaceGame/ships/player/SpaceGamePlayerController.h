#pragma once

#include <ioj/levels/control_context.h>
#include <SpaceGame/ships/player/PlayerHudLifecycle.h>
#include <SpaceGame/ships/player/PlayerModalUi.h>
#include <SpaceGamePresentation/support/logging/ActorLoggingConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/PlayerController.h>

#include "SpaceGamePlayerController.generated.h"

namespace ioj::host {
struct ViewerMotion;
}

class ACameraActor;
struct FLevelCompletion;

namespace ioj {
class UTestBatchGameUiData;
class ATestBatchOrchestrator;
class ATestSpaceShip;

UCLASS()
class SPACEGAME_API ASpaceGamePlayerController : public APlayerController {
    GENERATED_BODY()

    friend struct FPlayerHudLifecycle;
    friend struct FPlayerModalUi;
    friend class ATestBatchOrchestrator;
  public:
    using Pawn = ATestSpaceShip;
    ASpaceGamePlayerController();
    void pause_or_exit_viewer();
    void present_viewer_motion(host::ViewerMotion const& motion, double dt);
    void present_steering(TOptional<FVector2D> position);
    void Tick(float dt) override;
    void show_main_menu();
    auto activate_playerless_camera(ACameraActor& camera, levels::ControlContext context) -> bool;
    auto activate_run_controls() -> bool;
    [[nodiscard]] auto get_active_control_context() const noexcept -> levels::ControlContext {
        return control_context_;
    }
    [[nodiscard]] auto get_active_hud() const -> USimulationHudWidget const* {
        return hud_.get_hud();
    }
    [[nodiscard]] auto get_benchmark_hud() const -> UBenchmarkHudWidget* {
        return hud_.get_benchmark_hud();
    }
    [[nodiscard]] auto is_observer_movement_enabled() const noexcept -> bool {
        return control_context_ == levels::ControlContext::Observer;
    }

  protected:
    void BeginPlay() override;
    void OnPossess(APawn* in_pawn) override;
    void OnUnPossess() override;
    void EndPlay(EEndPlayReason::Type reason) override;
  private:
    enum class EPlayerControllerMode : uint8 { Gameplay, MainMenu };

    /* **************************************** */
    // Lifecycle and possession
    /* **************************************** */
    auto is_gameplay_mode() const -> bool {
        return mode_ == EPlayerControllerMode::Gameplay && !ending_play_;
    }

    auto can_activate_gameplay_control() const -> bool {
        return is_gameplay_mode() && !return_to_level_select_pending_ && !modal_ui_.has_modal() &&
               !modal_ui_.is_resume_pending();
    }

    void initialise_gameplay();
    void initialise_main_menu();
    void attach_ship(Pawn& ship);
    void detach_ship();
    void activate_ship_control();
    void on_player_ship_died();
    void bind_orchestrator_events();
    void unbind_orchestrator_events();

    /* **************************************** */
    // Input and presentation callbacks
    /* **************************************** */
    void set_observer_movement_speed(float speed);

    /* **************************************** */
    // Modal and simulation transitions
    /* **************************************** */
    void toggle_pause_game();
    void toggle_run_pause();
    void show_initial_pause_menu();
    auto open_pause_menu() -> bool;
    auto suspend_gameplay_for_modal() -> bool;
    void resume_game();
    void on_pause_menu_deactivated();
    void on_completion_menu_deactivated();
    void on_simulation_paused();
    void finish_level();
    void show_level_completion(FLevelCompletion const& completion);
    void return_to_level_select();
    void quit_game();

    /* **************************************** */
    // Diagnostics
    /* **************************************** */
    void screenshot_tick(float dt);

    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    TObjectPtr<UTestBatchGameUiData> ui_data{nullptr};

    UPROPERTY(Transient)
    FPlayerHudLifecycle hud_;
    UPROPERTY(Transient)
    FPlayerModalUi modal_ui_;

    auto set_control_context(levels::ControlContext context) -> bool;

    levels::ControlContext control_context_{levels::ControlContext::None};
    TWeakObjectPtr<ACameraActor> viewer_camera_;
    float viewer_speed_{50000.f};
    TWeakObjectPtr<ATestBatchOrchestrator> orchestrator_;
    EPlayerControllerMode mode_{EPlayerControllerMode::Gameplay};
    bool begin_play_finished_{false};
    bool ending_play_{false};
    bool return_to_level_select_pending_{false};

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Logging")
    FActorLoggingConfig log_config{1.f};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Screenshot")
    float screenshot_period{-1.f};
    float screenshot_accumulator{0.f};
};

} // namespace ioj
