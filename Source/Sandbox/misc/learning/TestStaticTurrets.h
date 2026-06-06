#pragma once

#include "TestTeam.h"

#include "Sandbox/logging/ActorLoggingConfig.h"

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

    static constexpr bool is_world_space{false};

    ATestStaticTurrets();

    void clear_runtime_state();
    void begin_play();
    void tick(float const dt);

    void spawn_instance(FTransform const& transform, ETestTeam const team);

    auto get_num_instances() const noexcept -> int32;
  protected:
    // Spawning
    void register_all_proxies_in_level();

    // Visuals
    void configure_ismc();

    // Searchng
    void perform_search();

    // Attacking
    void fire_at_enemies();

    // Debugging
    bool array_sizes_consistent() const;

    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<UTestStaticTurretsConfig> actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    // Data
    UPROPERTY()
    TArray<FGenerationIndex> indices{};
    UPROPERTY()
    TArray<FVector> locations{};

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    // Team
    UPROPERTY()
    TArray<ETestTeam> teams{};

    // Searching
    UPROPERTY(EditAnywhere, Category = "Performance", meta = (ClampMin = "1", UIMin = "1"))
    int32 search_slice_size{64};

    // Firing
    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY()
    FCountdownTimers laser_cooldowns;
    UPROPERTY()
    TArray<int32> indices_ready_to_fire;

    // Enemies
    UPROPERTY()
    TArray<FGenerationIndex> target_indices{};

    // Health
    UPROPERTY()
    TArray<int32> healths;

    // Debugging / logging
    UPROPERTY(EditAnywhere, Category = "Turrets")
    FActorLoggingConfig log_config{1.f};
};
