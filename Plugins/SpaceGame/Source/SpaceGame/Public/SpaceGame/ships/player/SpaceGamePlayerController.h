#pragma once

#include <CoreMinimal.h>
#include <GameFramework/PlayerController.h>
#include <SpaceGame/ships/player/PlayerControlContexts.h>
#include <SpaceGame/ships/player/PlayerHudLifecycle.h>
#include <SpaceGame/ships/player/PlayerModalUi.h>
#include <SpaceGamePresentation/support/logging/ActorLoggingConfig.h>
#include "SpaceGamePlayerController.generated.h"

class ATestBatchOrchestrator;
class ATestSpaceShip;
class ACameraActor;
class UEnhancedInputUserSettings;
struct FTestMissionCompletion;
struct FInputKeyEventArgs;
class UTestBatchGameUiData;

UCLASS()
class SPACEGAME_API ASpaceGamePlayerController : public APlayerController {
    GENERATED_BODY()

    friend struct FShipControlContext;
    friend struct FObserverControlContext;
    friend struct FBenchmarkControlContext;
    friend struct FGlobalPlayerInput;
    friend struct FPlayerHudLifecycle;
    friend struct FPlayerModalUi;
    friend struct FPlayerControllerTestAccess;
    friend class ATestBatchOrchestrator;
  public:
    using Pawn = ATestSpaceShip;
    ASpaceGamePlayerController();
    bool InputKey(FInputKeyEventArgs const& event_args) override;
    void SetupInputComponent() override;
    void Tick(float dt) override;
    void show_main_menu();
    auto activate_playerless_camera(ACameraActor& camera, EPlayerControlContext context) -> bool;
    [[nodiscard]] auto get_active_control_context() const noexcept -> EPlayerControlContext {
        return control_contexts_.get_active_context();
    }
    [[nodiscard]] auto get_active_hud() const -> USimulationHudWidget const* {
        return hud_.get_hud();
    }
    [[nodiscard]] auto get_benchmark_hud() const -> UBenchmarkHudWidget const* {
        return hud_.get_benchmark_hud();
    }
    [[nodiscard]] auto is_observer_movement_enabled() const noexcept -> bool {
        return get_active_control_context() == EPlayerControlContext::Observer &&
               control_contexts_.is_observer_bound();
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
    void apply_main_menu_input_mode();
    void apply_player_ship_flight_model_config();
    void apply_player_ship_flight_model_config(Pawn& ship) const;
    void on_player_ship_flight_model_selected();
    void attach_ship(Pawn& ship);
    void detach_ship();
    void activate_ship_control();
    void on_player_ship_died();
    void bind_orchestrator_events();
    void unbind_orchestrator_events();
    void on_orchestrator_reset(ATestBatchOrchestrator& orchestrator);

    /* **************************************** */
    // Input and presentation callbacks
    /* **************************************** */
    void initialise_input_user_settings();
    UFUNCTION()
    void on_input_user_settings_initialized(UEnhancedInputUserSettings const* settings);
    void set_observer_look_active(bool active);
    void set_observer_movement_speed(float speed);

    /* **************************************** */
    // Modal and simulation transitions
    /* **************************************** */
    void toggle_pause_game();
    void show_initial_pause_menu();
    auto open_pause_menu() -> bool;
    auto suspend_gameplay_for_modal() -> bool;
    void resume_game();
    void on_pause_menu_deactivated();
    void on_completion_menu_deactivated();
    void on_mission_completed(FTestMissionCompletion const& completion);
    void return_to_level_select();
    void quit_game();

    /* **************************************** */
    // Diagnostics
    /* **************************************** */
    void screenshot_tick(float dt);

    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    TObjectPtr<UTestBatchGameUiData> ui_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FSpaceShipControllerInputs input;
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FGlobalControlInputs global_input;
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FObserverControlInputs observer_input;
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FBenchmarkControlInputs benchmark_input;

    UPROPERTY(Transient)
    FPlayerHudLifecycle hud_;
    UPROPERTY(Transient)
    FPlayerModalUi modal_ui_;

    FPlayerControlContexts control_contexts_;
    TWeakObjectPtr<UEnhancedInputUserSettings> registered_input_settings_;
    TWeakObjectPtr<ATestBatchOrchestrator> orchestrator_;
    EPlayerControllerMode mode_{EPlayerControllerMode::Gameplay};
    bool begin_play_finished_{false};
    bool ending_play_{false};
    bool return_to_level_select_pending_{false};
    FTimerHandle initial_pause_timer_;
    FTimerHandle main_menu_input_timer_;

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Logging")
    FActorLoggingConfig log_config{1.f};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Screenshot")
    float screenshot_period{-1.f};
    float screenshot_accumulator{0.f};
};
