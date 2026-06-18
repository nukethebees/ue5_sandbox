#pragma once

#include <CoreMinimal.h>
#include <GameFramework/PlayerController.h>

#include "FunctionalTestPlayerController.generated.h"

class AActor;

UCLASS()
class AFunctionalTestPlayerController : public APlayerController {
    GENERATED_BODY()
  public:
    void BeginPlay() override;
  protected:
    void OnPossess(APawn* in_pawn) override;
    void OnUnPossess() override;
};
