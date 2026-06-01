#pragma once

#include "TestTeam.h"

#include "SandboxCore/countdown_timers.h"
#include "SandboxCore/generation_index.h"

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
    GENERATED_BODY()
  public:
    using Proxy = ATestStaticTurretsProxy;

    ATestStaticTurrets();

    void Tick(float dt) override;

    void spawn_instance(FTransform const& transform, ETestTeam const team);

    auto get_num_instances() const noexcept -> int32;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    // Spawning
    void register_all_proxies_in_level();

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

    // Data
    UPROPERTY(EditAnywhere, Category = "Turrets")
    TArray<FGenerationIndex> indices{};

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    // Team
    UPROPERTY(EditAnywhere, Category = "Turrets")
    TArray<ETestTeam> teams{};

    // Firing
    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ships")
    FCountdownTimers laser_cooldowns;
    UPROPERTY()
    TArray<int32> indices_ready_to_fire;

    // Health
    UPROPERTY()
    TArray<int32> healths;
};
