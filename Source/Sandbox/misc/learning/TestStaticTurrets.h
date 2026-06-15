#pragma once

#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/misc/learning/RegistryEntityHandle.h>
#include <Sandbox/misc/learning/TestEntityOwnerId.h>
#include <Sandbox/misc/learning/TestEntityRegistry.h>
#include <Sandbox/misc/learning/TestTeam.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <SandboxCore/countdown_timers.h>
#include <SandboxCore/generation_index.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

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

    void begin_tick();
    void tick(float const dt);
    void queue_commands();
    void resolve_hit_events();
    void update_entity_registry();
    void sync_from_registry();
    void update_visuals();
    void end_tick();

    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    // Checks
    void validate_array_sizes() const;
  protected:
    // Spawning
    void register_all_proxies_in_level();

    // Visuals
    void configure_ismc();

    // Entity data
    auto get_entity_data() const -> FTestEntityRegistryEntityData;

    // Searchng
    void perform_search();

    // Attacking
    void fire_at_enemies();

    // Misc
    void clear_tick_buffers();

    // Debugging
    void draw_debugging_shapes() const;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestStaticTurretsConfig> actor_config{nullptr};

    // Entity Data
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    TestEntityOwnerId owner_id{};
    TArray<FRegistryEntityHandle> entity_registry_handles{};

    // Location
    FVectors3f locations;

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    TArray<FTransform> ismc_transforms;

    // Team
    TArray<ETestTeam> teams{};

    // Searching
    UPROPERTY(EditAnywhere, Category = "Performance", meta = (ClampMin = "1", UIMin = "1"))
    int32 search_slice_size{64};

    // Firing
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> laser_actor{nullptr};

    FCountdownTimers laser_cooldowns;
    TArray<int32> indices_ready_to_fire;

    // Laser spawning
    FVectors3f new_laser_locations;
    FRotatorsf new_laser_rotations;
    TArray<FRegistryEntityHandle> new_laser_instigator_handles;

    // Enemies
    UPROPERTY()
    TArray<FRegistryEntityHandle> target_registry_handles{};

    // Health
    UPROPERTY()
    TArray<int32> healths;

    // Despawning
    UPROPERTY()
    TArray<int32> local_indices_to_remove;

    // Debugging / logging
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    FActorLoggingConfig log_config{1.f};
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool draw_target_arrows_enabled{false};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool draw_debug_entity_info_enabled{false};
};
