#pragma once

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipFighters.generated.h"

UCLASS()
class ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
  public:
    ATestCapitalShipFighters() = default;
  protected:
    UPROPERTY()
    TArray<FTransform> world_transforms;
};
