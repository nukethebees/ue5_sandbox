#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestLasers.generated.h"

class USceneComponent;
class UInstancedStaticMeshComponent;

class UTestLasersConfig;

UCLASS()
class ATestLasers : public AActor {
    GENERATED_BODY()
  public:
    ATestLasers();

    void PostInitializeComponents() override;
    void Tick(float dt) override;

    // Accessors
    auto get_num_instances() const noexcept -> int32;
    auto get_config() const -> UTestLasersConfig const* { return laser_config; }

    // Spawning / configuration
    void spawn_lasers(TConstArrayView<FTransform> const transforms);
  protected:
    void BeginPlay() override;

    // Spawning / Configuration
    void configure_ismc();

    // Movement
    void update_locations(float const dt);
    void handle_collisions(float const dt);

    // Visuals
    void update_ismc();

    // Lifetimes
    void tick_lifetimes(float const dt);
    void collect_old_instance_indices();

    // Debugging
    bool array_sizes_consistent() const;

    // Misc
    void clear_runtime_state();
    void remove_instances(TConstArrayView<int32> indices);

    UPROPERTY(EditAnywhere, Category = "Lasers")
    TObjectPtr<UTestLasersConfig> laser_config{nullptr};

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    UPROPERTY()
    TArray<FTransform> transforms;
    UPROPERTY()
    TArray<FVector> velocities;

    UPROPERTY()
    TArray<float> lifetimes;

    UPROPERTY()
    TArray<int32> to_remove;

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
