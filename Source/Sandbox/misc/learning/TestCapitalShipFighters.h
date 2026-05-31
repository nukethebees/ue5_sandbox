#pragma once

#include "TestTeam.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipFighters.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
  public:
    ATestCapitalShipFighters();

    void PostInitializeComponents() override;
    void Tick(float dt) override;

    void spawn_instance(FTransform const& transform);
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    // Misc
    void clear_runtime_state();

    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    UPROPERTY()
    TArray<FTransform> world_transforms;

    UPROPERTY(EditAnywhere, Category = "Ships")
    ETestTeam team{ETestTeam::neutral};
};
