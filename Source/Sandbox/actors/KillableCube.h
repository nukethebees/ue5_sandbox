#pragma once

#include "CoreMinimal.h"
#include "components/StaticMeshComponent.h"
#include "engine/World.h"
#include "GameFramework/Actor.h"
#include "Sandbox/actor_components/HealthComponent.h"

#include "KillableCube.generated.h"

UCLASS()
class SANDBOX_API AKillableCube : public AActor {
    GENERATED_BODY()
  public:
    AKillableCube();
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* mesh_component_{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHealthComponent* health_component_{nullptr};
  private:
    UFUNCTION()
    void on_death();
};
