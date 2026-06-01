#pragma once

#include "TestTeam.h"

#include "SandboxCore/countdown_timers.h"
#include "SandboxCore/generation_index.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipFighters.generated.h"

class UInstancedStaticMeshComponent;

class UTestCapitalShipFightersConfig;
class ATestLasers;
class ATestEntityRegistry;

UCLASS()
class ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
  public:
    ATestCapitalShipFighters();

    void PostInitializeComponents() override;
    void Tick(float dt) override;

    void spawn_instances(TConstArrayView<FTransform> const new_transforms,
                         TConstArrayView<ETestTeam> const new_teams);

    // Getters
    auto get_num_instances() const noexcept -> int32;
    bool array_sizes_consistent() const;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    // Movement
    void move_ships(float const dt);

    // Combat
    void handle_firing();

    // Misc
    void clear_runtime_state();

    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    // Config data
    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<UTestCapitalShipFightersConfig> actor_config{nullptr};

    // Runtime data
    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Ships")
    TArray<FGenerationIndex> indices;

    // Movement
    UPROPERTY(VisibleAnywhere, Category = "Ships")
    TArray<FTransform> world_transforms;
    UPROPERTY(VisibleAnywhere, Category = "Ships")
    TArray<FVector> velocities;

    // Team
    UPROPERTY(EditAnywhere, Category = "Ships")
    TArray<ETestTeam> teams{};

    // Combat
    UPROPERTY(VisibleAnywhere, Category = "Ships")
    TArray<int32> healths;

    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ships")
    FCountdownTimers laser_cooldowns;
    UPROPERTY()
    TArray<int32> indices_ready_to_fire;
};
