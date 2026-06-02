#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestTarget.generated.h"

class UStaticMeshComponent;

UCLASS()
class ATestTarget : public AActor {
    GENERATED_BODY()
  public:
    ATestTarget();
  protected:
    UPROPERTY(EditAnywhere, Category = "Fly")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
};
