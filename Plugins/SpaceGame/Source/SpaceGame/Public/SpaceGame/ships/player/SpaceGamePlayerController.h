#pragma once

#include <SpaceGame/ships/player/PlayerControlContext.h>
#include <SpaceGame/ships/player/ShipControlContext.h>
#include <SpaceGame/support/logging/ActorLoggingConfig.h>

#include <Components/SlateWrapperTypes.h>
#include <CoreMinimal.h>
#include <GameFramework/PlayerController.h>

#include "SpaceGamePlayerController.generated.h"

class ATestBatchOrchestrator;
class ATestSpaceShip;
struct FTestMissionCompletion;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UShipHudWidget;
class UTestBatchGameUiData;

namespace ml::ioj {
class UGameUiRootLayout;
class ULevelCompletionWidget;
class UPauseMenuWidget;
}

UCLASS()
class ASpaceGamePlayerController : public APlayerController {
    GENERATED_BODY()

    friend struct FShipControlContext;
  public:
    using Pawn = ATestSpaceShip;

    ASpaceGamePlayerController();

    void SetupInputComponent() override;
    void Tick(float dt) override;

    void show_main_menu();

    [[nodiscard]] auto get_active_control_context() const noexcept -> EPlayerControlContext {
        return active_control_context_;
    }
  protected:
    void BeginPlay() override;
    void OnPossess(APawn* in_pawn) override;
    void OnUnPossess() override;
    void EndPlay(EEndPlayReason::Type reason) override;
  private:
    // Input orchestration
    auto initialise_global_input(UEnhancedInputComponent& input_component,
                                 UEnhancedInputLocalPlayerSubsystem& input_subsystem) -> bool;
    void shutdown_global_input();
    auto set_control_context(EPlayerControlContext context) -> bool;
    auto can_bind_context(EPlayerControlContext context) const -> bool;
    auto bind_context(EPlayerControlContext context) -> bool;
    void unbind_context(EPlayerControlContext context);
    void on_ship_mapping_context_changed(UInputMappingContext const& context);
    void toggle_pause_game();

    // UI and simulation transitions
    void initialise_main_menu();
    void apply_main_menu_input_mode();
    void initialise_gameplay();
    auto initialise_ui_root() -> bool;
    void shutdown_ui_root();
    void initialise_hud();
    void hide_hud_for_modal();
    void restore_hud_after_modal();
    auto suspend_gameplay_for_modal() -> bool;
    void resume_game();
    void on_pause_menu_deactivated();
    void on_completion_menu_deactivated();
    void on_mission_completed(FTestMissionCompletion const& completion);
    void return_to_level_select();
    void quit_game();
    void detach_modal_callbacks();
    void select_main_menu_camera();
    void bind_orchestrator_events();
    void on_orchestrator_reset(ATestBatchOrchestrator& orchestrator);

    // Player lifecycle
    void on_player_ship_died();

    // Misc
    void screenshot_tick(float dt);

    TWeakObjectPtr<ATestBatchOrchestrator> hud_orchestrator;

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|UI")
    TObjectPtr<UShipHudWidget> hud_widget{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    TObjectPtr<UTestBatchGameUiData> ui_data{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|UI")
    TObjectPtr<ml::ioj::UGameUiRootLayout> ui_root{nullptr};

    UPROPERTY(Transient)
    TObjectPtr<ml::ioj::UPauseMenuWidget> pause_menu{nullptr};

    UPROPERTY(Transient)
    TObjectPtr<ml::ioj::ULevelCompletionWidget> completion_menu{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FSpaceShipControllerInputs input;

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FGlobalControlInputs global_input;

    FShipControlContext ship_control_context_;

    TWeakObjectPtr<UEnhancedInputComponent> global_input_component_;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> global_input_subsystem_;
    uint32 global_input_binding_handle_{0};
    EPlayerControlContext active_control_context_{EPlayerControlContext::None};
    bool global_input_bound_{false};
    bool begin_play_finished_{false};
    bool main_menu_requested_{false};
    bool restore_ship_controls_after_modal_{false};
    bool modal_resume_pending_{false};
    bool hud_restore_pending_{false};
    bool return_to_level_select_pending_{false};
    bool shutting_down_ui_{false};
    ESlateVisibility hud_visibility_before_modal_{ESlateVisibility::Visible};

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Logging")
    FActorLoggingConfig log_config{1.f};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Screenshot")
    float screenshot_period{-1.f};

    float screenshot_accumulator{0.f};
};
