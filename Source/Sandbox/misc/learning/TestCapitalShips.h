#pragma once

#include <SandboxCore/Public/generation_index.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShips.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;

class UTestCapitalShipsConfig;
class ATestCapitalShipProxy;
class ATestCapitalShipFighters;

UCLASS()
class ATestCapitalShips : public AActor {
  public:
    GENERATED_BODY()

    ATestCapitalShips();

    void Tick(float dt) override;

    void spawn_ship(FTransform const& transform);
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UTestCapitalShipsConfig> ship_config{nullptr};

    UPROPERTY()
    TObjectPtr<ATestCapitalShipFighters> fighters_actor{nullptr};

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY()
    TArray<TObjectPtr<UBoxComponent>> collision_boxes;

    UPROPERTY()
    TArray<FTransform> transforms;

    UPROPERTY()
    TArray<TWeakObjectPtr<ATestCapitalShips>> targets;
    UPROPERTY()
    TArray<FGenerationIndex> target_entity_indexes;
};
