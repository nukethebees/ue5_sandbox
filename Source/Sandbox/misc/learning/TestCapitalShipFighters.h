#pragma once

#include "TestTeam.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipFighters.generated.h"

class UInstancedStaticMeshComponent;

class UTestCapitalShipFightersConfig;

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

    // Getters
    auto get_num_instances() const -> int32;

    // Movement
    void move_ships(float const dt);

    // Misc
    void clear_runtime_state();

    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<UTestCapitalShipFightersConfig> actor_config{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Ships")
    TArray<FTransform> world_transforms;

    UPROPERTY(VisibleAnywhere, Category = "Ships")
    TArray<int32> healths;

    UPROPERTY(EditAnywhere, Category = "Ships")
    ETestTeam team{ETestTeam::neutral};
};
