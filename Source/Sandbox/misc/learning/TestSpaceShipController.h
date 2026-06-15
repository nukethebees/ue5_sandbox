#pragma once

#include <Sandbox/input/EnhancedInputMixin.hpp>
#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/logging/LogMsgMixin.hpp>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/players/BarrelRollInputData.h>
#include <Sandbox/players/LaserFiringMode.h>
#include <Sandbox/players/SpaceShipControllerInputs.h>

#include <CoreMinimal.h>
#include <GameFramework/PlayerController.h>

#include "TestSpaceShipController.generated.h"

class UShipHudWidget;
class ATestSpaceShip;
class ATestMissionManager;

UCLASS()
class ATestSpaceShipController
    : public APlayerController
    , public ml::EnhancedInputMixin
    , public ml::LogMsgMixin<"SpaceShipController", LogSandboxController> {
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
    void EndPlay(EEndPlayReason::Type const reason);

    void initialise_hud();
    void update_crosshair_positions(ATestSpaceShip const& ship);
    void update_lock_on_widget(ATestSpaceShip const& ship);

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
    UFUNCTION()
    void on_lock_on_acquired(AActor* target);

    // Mission
    void on_mission_manager_ready(ATestMissionManager const& manager);
    void initialise_from_mission_manager(ATestMissionManager const& manager);
    void on_mission_update(ATestMissionManager const& manager);
    void on_mission_ended(ATestMissionManager const& manager);
    auto make_mission_status_message(ATestMissionManager const& manager) const -> FString;

    // UI
    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    TSubclassOf<UShipHudWidget> hud_widget_class;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|UI")
    UShipHudWidget* hud_widget{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    float near_cursor_distance{3000.f};
    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    float far_cursor_distance{6000.f};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FBarrelRollInputData barrel_roll_input{};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FSpaceShipControllerInputs input;

    // Mission state
    UPROPERTY(EditAnywhere, Category = "Sandbox|Mission")
    TObjectPtr<ATestMissionManager> mission_manager{nullptr};
    FDelegateHandle on_mission_ended_handle;
    FDelegateHandle on_mission_update_handle;
    FDelegateHandle on_mission_manager_ready_handle;

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Logging")
    FActorLoggingConfig log_config{1.f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debug")
    bool debug_crosshair{false};
#endif
};
