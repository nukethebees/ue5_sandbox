#pragma once

#include "Sandbox/input/EnhancedInputMixin.hpp"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/playable/space_ship/SpaceShipControllerInputs.h"

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "SpaceShipController.generated.h"

class UShipHudWidget;

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

    // UI
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UShipHudWidget> hud_widget_class;
    UPROPERTY(VisibleAnywhere, Category = "UI")
    UShipHudWidget* hud_widget{nullptr};

    // Movement
    UFUNCTION()
    void turn(FInputActionValue const& value);
    UFUNCTION()
    void turn_completed(FInputActionValue const& value);
    UFUNCTION()
    void fire_laser(FInputActionValue const& value);
    UFUNCTION()
    void fire_bomb(FInputActionValue const& value);
    UFUNCTION()
    void boost(FInputActionValue const& value);
    UFUNCTION()
    void brake(FInputActionValue const& value);

    UPROPERTY(EditAnywhere, Category = "Input")
    FSpaceShipControllerInputs input;
};
