#pragma once

#include <SandboxGameShared/input/EnhancedInputMixin.hpp>
#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/players/SpaceShipControllerInputs.h>

#include <CoreMinimal.h>
#include <GameFramework/PlayerController.h>

#include "TestSpaceShipController.generated.h"

class UShipHudWidget;
class UTestBatchGameUiData;
class UInputAction;
class ATestSpaceShip;
class ATestBatchOrchestrator;

UCLASS()
class ATestSpaceShipController
    : public APlayerController
    , public ml::EnhancedInputMixin {
    GENERATED_BODY()
  public:
    using Pawn = ATestSpaceShip;

    ATestSpaceShipController();

    void SetupInputComponent() override;
    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnPossess(APawn* in_pawn) override;
    void OnUnPossess() override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    void initialise_hud();

    auto get_pawn() -> Pawn&;

    // Input
    void set_mapping_context(UInputMappingContext const* context);

    // Movement
    UFUNCTION()
    void set_move_input(FInputActionValue const& value);
    UFUNCTION()
    void move_completed();
    UFUNCTION()
    void set_lateral_move_input(FInputActionValue const& value);
    UFUNCTION()
    void lateral_move_completed();
    UFUNCTION()
    void set_vertical_move_input(FInputActionValue const& value);
    UFUNCTION()
    void vertical_move_completed();
    UFUNCTION()
    void set_ship_2d_control_started();
    UFUNCTION()
    void set_ship_2d_control(FInputActionValue const& value);
    UFUNCTION()
    void ship_2d_control_completed();
    UFUNCTION()
    void set_ship_1d_control_x(FInputActionValue const& value);
    UFUNCTION()
    void set_ship_1d_control_y(FInputActionValue const& value);
    UFUNCTION()
    void cycle_next_control_mode();
    UFUNCTION()
    void cycle_previous_control_mode();
    UFUNCTION()
    void start_sampling();
    UFUNCTION()
    void stop_sampling();
    UFUNCTION()
    void turn(FInputActionValue const& value);
    UFUNCTION()
    void turn_completed(FInputActionValue const& value);
    UFUNCTION()
    void start_roll(FInputActionValue const& value);
    UFUNCTION()
    void roll(FInputActionValue const& value);
    UFUNCTION()
    void stop_roll(FInputActionValue const& value);
    UFUNCTION()
    void start_boost(FInputActionValue const& value);
    UFUNCTION()
    void stop_boost(FInputActionValue const& value);
    UFUNCTION()
    void start_brake(FInputActionValue const& value);
    UFUNCTION()
    void stop_brake(FInputActionValue const& value);
    UFUNCTION()
    void cycle_input_mapping_context();

    // Combat
    UFUNCTION()
    void start_fire_laser();
    UFUNCTION()
    void stop_fire_laser();
    UFUNCTION()
    void fire_bomb(FInputActionValue const& value);

    UFUNCTION()
    void cycle_prev_fire_rate();
    UFUNCTION()
    void cycle_next_fire_rate();

    // Player lifecycle
    UFUNCTION()
    void on_player_ship_died();

    // Misc
    void screenshot_tick(float dt);

    // UI
    TWeakObjectPtr<ATestBatchOrchestrator> hud_orchestrator;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|UI")
    UShipHudWidget* hud_widget{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    TObjectPtr<UTestBatchGameUiData> ui_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FSpaceShipControllerInputs input;

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* lateral_move_input{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* vertical_move_input{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* sample_and_hold_input{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* ship_2d_control{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* ship_1d_control_x{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* ship_1d_control_y{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* cycle_next_control_mode_input{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* cycle_previous_control_mode_input{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    int32 input_mapping_context_index{0};

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Logging")
    FActorLoggingConfig log_config{1.f};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Screenshot")
    float screenshot_period{-1.f};
    float screenshot_accumulator{0.f};
};
