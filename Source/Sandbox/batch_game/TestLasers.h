#pragma once

#include <Sandbox/batch_game/test_entity_registry/DamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <SandboxCore/generation_index.h>
#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include <Math/Color.h>

#include "TestLasers.generated.h"

class USceneComponent;
class UInstancedStaticMeshComponent;
class UActorComponent;

class UTestLasersConfig;
class ATestEntityRegistry;

struct FTestLasersSpawnRequests : public ml::FSoAArrayMixin {
    FVectors3f locations;
    FRotatorsf rotations;
    TArray<int32> damages;
    TArray<float> speeds;
    TArray<float> max_distances;
    TArray<FRegistryEntityHandle> instigator_handles;
    TArray<FLinearColor> colours;

    void set_damages(int32 const value);
    void set_speeds(float const value);
    void set_max_distances(float const value);
    void set_colours(FLinearColor const value);

    void append_from(FTestLasersSpawnRequests const& other);

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.locations,
                                         self.rotations,
                                         self.damages,
                                         self.speeds,
                                         self.max_distances,
                                         self.instigator_handles,
                                         self.colours);
    }
};

struct FTestLasersHitDetails : public ml::FSoAArrayMixin {
    FVectors3f locations;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.locations);
    }
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
    void prepare_ismc_transforms();
    void update_ismc();
    void spawn_hit_effects();

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
    TArray<FInstancedStaticMeshInstanceData> ismc_data;
    FTestLasersHitDetails hit_details;

    // Transform
    FVectors3f locations;
    FRotatorsf rotations;
    FVectors3f velocities;

    TArray<int32> damages;

    // Lifetime
    TArray<float> lifetimes_remaining;
    TArray<FRegistryEntityHandle> instigator_handles;

    // Spawning
    FTestLasersSpawnRequests pending_spawns;

    // Removal
    TArray<int32> to_remove;

    // Damage transaction
    UnresolvedDamageEvents damage_events;

    // Debugging
    bool have_warned_hit_effect{false};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Lasers")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Lasers")
    bool debugging_shapes_enabled{false};
#endif
};
