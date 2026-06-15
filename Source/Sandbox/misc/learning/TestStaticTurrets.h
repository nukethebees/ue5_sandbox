#pragma once

#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/misc/learning/RegistryEntityHandle.h>
#include <Sandbox/misc/learning/TestEntityOwnerId.h>
#include <Sandbox/misc/learning/TestEntityRegistry.h>
#include <Sandbox/misc/learning/TestTeam.h>

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

    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<UTestStaticTurretsConfig> actor_config{nullptr};

    // Entity Data
    TestEntityOwnerId owner_id{};

    UPROPERTY()
    TArray<FRegistryEntityHandle> entity_indices{};
    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
    UPROPERTY()
    TArray<int32> local_indices_to_remove;

    // Location
    UPROPERTY()
    FVectors3f locations;

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

    FVectors3f new_laser_locations;
    FRotatorsf new_laser_rotations;
    TArray<FRegistryEntityHandle> new_laser_instigator_handles;

    // Enemies
    UPROPERTY()
    TArray<FRegistryEntityHandle> target_indices{};

    // Health
    UPROPERTY()
    TArray<int32> healths;

    // Debugging / logging
    UPROPERTY(EditAnywhere, Category = "Turrets")
    FActorLoggingConfig log_config{1.f};
};
