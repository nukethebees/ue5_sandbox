#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestFighterAttackDriver.generated.h"

class UInstancedStaticMeshComponent;

class ATestCapitalShipProxy;

UCLASS()
class ATestFighterAttackDriver : public AActor {
    GENERATED_BODY()
  public:
      ATestFighterAttackDriver();

    UPROPERTY(EditAnywhere, Category = "Test")
    TObjectPtr<ATestCapitalShipProxy> hero{nullptr};

    UPROPERTY(EditAnywhere, Category = "Test")
    TObjectPtr<ATestCapitalShipProxy> enemy{nullptr};
};
