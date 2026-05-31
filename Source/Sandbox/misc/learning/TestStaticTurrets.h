#pragma once

#include "TestTeam.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestStaticTurrets.generated.h"

class UInstancedStaticMeshComponent;

class ATestStaticTurretsProxy;
class UTestStaticTurretsConfig;

UCLASS()
class ATestStaticTurrets : public AActor {
  public:
    GENERATED_BODY()

    ATestStaticTurrets();

    void Tick(float dt) override;

    void spawn_instance(FTransform const& transform);
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    void configure_ismc();

    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<UTestStaticTurretsConfig> actor_config{nullptr};

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    UPROPERTY(EditAnywhere, Category = "Turrets")
    ETestTeam team{ETestTeam::neutral};
};
