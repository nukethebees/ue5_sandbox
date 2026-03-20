#pragma once

#include "Sandbox/input/EnhancedInputMixin.hpp"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/LaserFiringMode.h"
#include "Sandbox/players/SpaceShipControllerInputs.h"

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "SpaceShipController.generated.h"

class UShipHudWidget;

USTRUCT(BlueprintType)
struct FBarrelRollInputData {
    GENERATED_BODY()

    FBarrelRollInputData() = default;

    UPROPERTY(EditAnywhere)
    float input_strength_threshold{0.7f};
    UPROPERTY(EditAnywhere)
    float input_time_threshold{0.25f};
    UPROPERTY(VisibleAnywhere)
    bool threshold_crossed_this_input{false};
    UPROPERTY(VisibleAnywhere)
    float last_crossing_time{0.f};
};

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
    void on_laser_firing_mode_changed(ELaserFiringMode mode);

    // UI
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UShipHudWidget> hud_widget_class;
    UPROPERTY(VisibleAnywhere, Category = "UI")
    UShipHudWidget* hud_widget{nullptr};
    UPROPERTY(EditAnywhere, Category = "UI")
    float near_cursor_distance{3000.f};
    UPROPERTY(EditAnywhere, Category = "UI")
    float far_cursor_distance{6000.f};

    UPROPERTY(EditAnywhere, Category = "Input")
    FBarrelRollInputData barrel_roll_input{};

    UPROPERTY(EditAnywhere, Category = "Input")
    FSpaceShipControllerInputs input;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Debug")
    float seconds_since_last_log{0};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float seconds_per_log{0.75f};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool debug_crosshair{false};
#endif
};
