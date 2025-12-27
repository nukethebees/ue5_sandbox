#pragma once

#include "CoreMinimal.h"
#include "engine/World.h"
#include "GameFramework/Actor.h"

#include "Sandbox/health/interfaces/DeathHandler.h"

#include "KillableCube.generated.h"

class UStaticMeshComponent;

class UHealthComponent;

UCLASS()
class SANDBOX_API AKillableCube
    : public AActor
    , public IDeathHandler {
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
    virtual void handle_death() override;
};
