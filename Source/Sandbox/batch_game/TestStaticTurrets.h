#pragma once

#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <SandboxCore/countdown_timers.h>
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
class SANDBOX_API ATestStaticTurrets : public AActor {
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

    auto get_target_handles() const -> TConstArrayView<FRegistryEntityHandle>;

    auto get_entity_registry() const -> ATestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(ATestEntityRegistry& reg) { entity_registry = &reg; }

    auto get_laser_actor() const -> ATestLasers const* { return laser_actor; }
    void set_laser_actor(ATestLasers& new_ref) { laser_actor = &new_ref; }

    // Checks
    void validate_array_sizes() const;
  protected:
    // Spawning
    void register_all_proxies_in_level();

    // Visuals
    void configure_ismc();

    // Entity data
    void prepare_entity_update_data();

    // Searchng
    void perform_search();

    // Attacking
    void fire_at_enemies();
    auto get_disengage_radius() const -> float;

    // Death handling
    void handle_dead_entities();
    void trigger_death_effects();

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
    TArray<FRegistryEntityHandle> entity_handles{};
    EntityDeathInfo entity_death_info;
    FTestEntityRegistryEntityData entity_update_data;

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
    FTestLasersSpawnRequests new_lasers;

    // Enemies
    UPROPERTY()
    TArray<FRegistryEntityHandle> target_handles{};

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
