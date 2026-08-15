#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/test_entity_registry/DirectDamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/TestLasersSoA.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>
#include <Engine/HitResult.h>
#include <GameFramework/Actor.h>
#include <Math/Color.h>
#include <SandboxCore/generation_index.h>

#include "TestLasers.generated.h"

class USceneComponent;
class UInstancedStaticMeshComponent;
class UActorComponent;
class UWorld;

class UTestLasersConfig;
struct FTestEntityRegistry;
class ATestBatchOrchestrator;
namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_lasers {
struct ThreadLocalCollisionData {
    TArray<FHitResult> hits;
    DirectDamageEvents damage_events;
    TArray<int32> to_remove;
    HitDetails hit_details;
};
}

UCLASS()
class ATestLasers : public AActor {
    GENERATED_BODY()
  public:
    using SpawnRequests = ml::test_lasers::SpawnRequests;
    using Entities = ml::test_lasers::Entities;
    using HitDetails = ml::test_lasers::HitDetails;
    using ThreadLocalCollisionData = ml::test_lasers::ThreadLocalCollisionData;

    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{5}; // RGB[3], lifetime, spawn time

    ATestLasers();

    void clear_runtime_state();
    void begin_play();
    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;

    void begin_tick();
    void commit_spawns();
    void simulate(float const dt);
    void update_visual_data();
    void commit_visual_data();
    void end_tick();

    // Accessors
    auto get_num_instances() const noexcept -> int32;
    auto get_config() const -> UTestLasersConfig const* { return actor_config; }
    void set_actor_config(UTestLasersConfig* const new_config) noexcept {
        actor_config = new_config;
    }

    auto get_entity_registry() const -> FTestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(FTestEntityRegistry& reg) { entity_registry = &reg; }
    void set_spatial_query_manager(ml::FSpatialQueryManager& manager) { query_manager = &manager; }

    // Spawning / configuration
    void queue_laser_spawns(SpawnRequests const& spawn_data);

    // Checks
    void validate_array_sizes() const;
  protected:
    // Spawning / Configuration
    void preallocate_instances();
    void process_pending_spawns();

    // Movement
    void update_locations(float const dt);
    void handle_collisions(float const dt);

    // Visuals
    void configure_ismc();
    void prepare_ismc_transforms();
    void update_ismc();
    void spawn_hit_effects();

    // Lifetimes
    void tick_lifetimes(float const dt);
    void collect_old_instance_indices();

    // Collision
    static void check_collision_thread(int32 const job_index,
                                       int32 const updates_per_slice,
                                       float const dt,
                                       ThreadLocalCollisionData& data,
                                       ATestLasers const& lasers);

    // Misc
    void remove_instances(TConstArrayView<int32> indices);
    void clear_spawn_buffers();
    void clear_hit_buffers();

    FTestEntityRegistry* entity_registry{nullptr};
    ml::FSpatialQueryManager* query_manager{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestLasersConfig> actor_config{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 n_preallocated_instances{5000};

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    Entities entities;

    // Spawning
    ml::test_lasers::SpawnRequests pending_spawns;
    TArray<float> custom_data_spawn_buffer;
    TArray<FTransform> dummy_transforms_spawn_buffer;

    // Removal
    TArray<int32> to_remove;

    // Damage transaction
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 collision_jobs{8};
    TArray<ThreadLocalCollisionData> thread_local_collision_data;
    TArray<FHitResult> collision_hits;
    DirectDamageEvents collision_damage_events;

    // Hits
    HitDetails hit_details;

    // Debugging
    bool have_warned_hit_effect{false};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Lasers")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Lasers")
    bool debugging_shapes_enabled{false};
#endif
};
