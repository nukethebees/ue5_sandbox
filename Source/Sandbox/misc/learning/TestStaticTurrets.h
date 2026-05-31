#pragma once

#include "TestTeam.h"

#include "SandboxCore/countdown_timers.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestStaticTurrets.generated.h"

class UInstancedStaticMeshComponent;

class ATestStaticTurretsProxy;
class UTestStaticTurretsConfig;
class ATestLasers;
class ATestEntityRegistry;

UCLASS()
class ATestStaticTurrets : public AActor {
  public:
    GENERATED_BODY()

    ATestStaticTurrets();

    void Tick(float dt) override;

    void spawn_instance(FTransform const& transform);

    auto get_num_instances() const noexcept -> int32;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    // Visuals
    void configure_ismc();

    // Searchng
    void perform_search();

    // Attacking
    void fire_at_enemies();

    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<UTestStaticTurretsConfig> actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    // Team
    UPROPERTY(EditAnywhere, Category = "Turrets")
    ETestTeam team{ETestTeam::neutral};

    // Firing
    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ships")
    FCountdownTimers laser_cooldowns;
    UPROPERTY()
    TArray<int32> indices_ready_to_fire;
};
