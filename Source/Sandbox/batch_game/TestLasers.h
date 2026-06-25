#pragma once

#include <Sandbox/batch_game/test_entity_registry/DamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <SandboxCore/generation_index.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestLasers.generated.h"

class USceneComponent;
class UInstancedStaticMeshComponent;
class UActorComponent;

class UTestLasersConfig;
class ATestEntityRegistry;

struct FTestLasersSpawnRequests {
    FVectors3f locations;
    FRotatorsf rotations;
    TArray<int32> damages;
    TArray<FRegistryEntityHandle> instigator_handles;
    
    void validate_array_sizes() const;
    void reset();
    auto num() const noexcept -> int32;
    void reserve(int32 const count);
    void add_uninitialised(int32 const count);
};

UCLASS()
class ATestLasers : public AActor {
    GENERATED_BODY()
  public:
    static constexpr bool is_world_space{false};

    ATestLasers();

    void clear_runtime_state();
    void begin_play();

    void begin_tick();
    void commit_spawns();
    void simulate(float const dt);
    void update_visuals();
    void end_tick();

    // Accessors
    auto get_num_instances() const noexcept -> int32;
    auto get_config() const -> UTestLasersConfig const* { return actor_config; }

    auto get_entity_registry() const -> ATestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(ATestEntityRegistry& reg) { entity_registry = &reg; }

    // Spawning / configuration
    void spawn_lasers(FTestLasersSpawnRequests const& spawn_data);

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
    void update_ismc_transforms();
    void update_ismc();

    // Lifetimes
    void tick_lifetimes(float const dt);
    void collect_old_instance_indices();

    // Misc
    void remove_instances(TConstArrayView<int32> indices);
    void clear_spawn_buffers();
    void clear_hit_buffers();

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestLasersConfig> actor_config{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 n_preallocated_instances{5000};

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY()
    TArray<FTransform> ismc_transforms;

    // Transform
    UPROPERTY()
    FVectors3f locations;
    UPROPERTY()
    FRotatorsf rotations;
    UPROPERTY()
    FVectors3f velocities;

    UPROPERTY()
    TArray<int32> damages;

    // Lifetime
    UPROPERTY()
    TArray<float> lifetimes;

    UPROPERTY()
    TArray<FRegistryEntityHandle> instigator_handles;

    // Spawning
    FTestLasersSpawnRequests pending_spawns;

    // Removal
    UPROPERTY()
    TArray<int32> to_remove;

    // Damage transaction
    UnresolvedDamageEvents damage_events;

#if WITH_EDITORONLY_DATA
    // Debugging
    UPROPERTY(EditAnywhere, Category = "Lasers")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Lasers")
    bool debugging_shapes_enabled{false};
#endif
};
