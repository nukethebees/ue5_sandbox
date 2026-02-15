#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SandboxSkybox.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;

UCLASS()
class ASandboxSkybox : public AActor {
    GENERATED_BODY()

    ASandboxSkybox();
  protected:
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UStaticMeshComponent* mesh{nullptr};
};
