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

UCLASS()
class ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
  public:
    ATestCapitalShipFighters();

    void PostInitializeComponents() override;
    void Tick(float dt) override;

    void spawn_instance(FTransform const& transform);

    // Getters
    auto get_num_instances() const noexcept -> int32;
    auto get_team() const noexcept -> ETestTeam;
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

    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<UTestCapitalShipFightersConfig> actor_config{nullptr};

    // Movement
    UPROPERTY(VisibleAnywhere, Category = "Ships")
    TArray<FTransform> world_transforms;

    // Team
    UPROPERTY(EditAnywhere, Category = "Ships")
    ETestTeam team{ETestTeam::neutral};

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
