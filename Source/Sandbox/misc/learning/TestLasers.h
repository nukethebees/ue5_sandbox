#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

#include "SandboxCore/generation_index.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestLasers.generated.h"

class USceneComponent;
class UInstancedStaticMeshComponent;

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
    void tick(float const dt);

    // Accessors
    auto get_num_instances() const noexcept -> int32;
    auto get_config() const -> UTestLasersConfig const* { return laser_config; }

    // Spawning / configuration
    void spawn_lasers(TConstArrayView<FTransform> const transforms);
  protected:
    // Spawning / Configuration
    void preallocate_instances();
    void process_pending_spawns();

    // Movement
    void update_locations(float const dt);
    void handle_collisions(float const dt);

    // Visuals
    void configure_ismc();
    void update_ismc();

    // Lifetimes
    void tick_lifetimes(float const dt);
    void collect_old_instance_indices();

    // Debugging
    bool array_sizes_consistent() const;

    // Misc
    void remove_instances(TConstArrayView<int32> indices);

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestLasersConfig> laser_config{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 n_preallocated_instances{5000};

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    UPROPERTY()
    TArray<FTransform> transforms;
    UPROPERTY()
    TArray<FVector> velocities;

    UPROPERTY()
    TArray<float> lifetimes;

    UPROPERTY()
    TArray<FTransform> transforms_to_add;
    UPROPERTY()
    TArray<int32> to_remove;
    UPROPERTY()
    TArray<FGenerationIndex> hit_entity_queue;
    UPROPERTY()
    TArray<int32> hit_damage_queue;

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
