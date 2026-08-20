#pragma once

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/BarrelRollInputData.h"
#include "Sandbox/players/LaserFiringState.h"
#include "Sandbox/players/SpaceShipControllerInputs.h"
#include "Sandbox/ui/HudCrosshairDistances.h"
#include "SandboxGameShared/input/EnhancedInputMixin.hpp"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "SpaceShipController.generated.h"

class UShipHudWidget;
class ASpaceShip;
class UTestBatchGameUiData;

UCLASS()
class ASpaceShipController
    : public APlayerController
    , public ml::EnhancedInputMixin
    , public ml::LogMsgMixin<"SpaceShipController", LogSandboxController> {
    GENERATED_BODY()

    ASpaceShipController();

    void SetupInputComponent() override;
    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnPossess(APawn* in_pawn) override;
    void OnUnPossess() override;

    void initialise_hud();
    void update_crosshair_positions(ASpaceShip const& ship);
    void update_lock_on_widget(ASpaceShip const& ship);

#if WITH_EDITOR
    auto can_log() const -> bool { return seconds_since_last_log >= seconds_per_log; }
#endif

    // Movement
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
    void barrel_roll(FInputActionValue const& value);
    UFUNCTION()
    void start_boost(FInputActionValue const& value);
    UFUNCTION()
    void stop_boost(FInputActionValue const& value);
    UFUNCTION()
    void start_brake(FInputActionValue const& value);
    UFUNCTION()
    void stop_brake(FInputActionValue const& value);

    // Combat
    UFUNCTION()
    void start_fire_laser();
    UFUNCTION()
    void stop_fire_laser();
    UFUNCTION()
    void fire_bomb(FInputActionValue const& value);

    // UI
    UFUNCTION()
    void on_laser_firing_mode_changed(ELaserFiringState mode);
    UFUNCTION()
    void on_lock_on_acquired(AActor* target);

    // UI
    UPROPERTY(VisibleAnywhere, Category = "UI")
    UShipHudWidget* hud_widget{nullptr};
    UPROPERTY(EditAnywhere, Category = "UI")
    TObjectPtr<UTestBatchGameUiData> ui_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Input")
    FBarrelRollInputData barrel_roll_input{};

    UPROPERTY(EditAnywhere, Category = "Input")
    FSpaceShipControllerInputs input;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Debug")
    float seconds_since_last_log{0};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float seconds_per_log{0.75f};
#endif
};
