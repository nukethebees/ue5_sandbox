#pragma once

#include "Sandbox/input/mixins/EnhancedInputMixin.hpp"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/playable/space_ship/SpaceShipControllerInputs.h"

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "SpaceShipController.generated.h"

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

    // Movement
    UFUNCTION()
    void turn(FInputActionValue const& value);
    UFUNCTION()
    void turn_completed(FInputActionValue const& value);
    UFUNCTION()
    void fire_laser(FInputActionValue const& value);
    UFUNCTION()
    void fire_bomb(FInputActionValue const& value);

    UPROPERTY(EditAnywhere, Category = "Input")
    FSpaceShipControllerInputs input;
};
