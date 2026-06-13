#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

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
    void tick(float const dt);
    void update_visuals();
    void end_tick();

    // Accessors
    auto get_num_instances() const noexcept -> int32;
    auto get_config() const -> UTestLasersConfig const* { return actor_config; }

    // Spawning / configuration
    void spawn_lasers(TVectors3View<float const> const locations,
                      TRotatorsView<float const> const rotations);

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

    // Lifetime
    UPROPERTY()
    TArray<float> lifetimes;

    // Spawning
    UPROPERTY()
    FVectors3f locations_to_add;
    UPROPERTY()
    FRotatorsf rotations_to_add;
    UPROPERTY()
    TArray<int32> to_remove;

    // Damage transaction
    UPROPERTY()
    TArray<int32> hit_damage_queue;
    UPROPERTY()
    TArray<AActor*> hit_actor_queue;
    UPROPERTY()
    TArray<UActorComponent*> hit_component_queue;
    UPROPERTY()
    TArray<int32> hit_item_queue;

#if WITH_EDITORONLY_DATA
    // Debugging
    UPROPERTY(EditAnywhere, Category = "Lasers")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Lasers")
    bool debugging_shapes_enabled{false};

    UPROPERTY(VisibleAnywhere, Category = "Lasers")
    int32 dbg_n_instances{0};
#endif
};
