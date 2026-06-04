#pragma once

#include "Sandbox/logging/ActorLoggingConfig.h"

#include "SandboxCore/countdown_timers.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestTubeSpinners.generated.h"

class UInstancedStaticMeshComponent;

class ATestTubeSpinnerProxy;
class UTestTubeSpinnersConfig;
class ATestLasers;

UCLASS()
class ATestTubeSpinners : public AActor {
    GENERATED_BODY()
  public:
    using Proxy = ATestTubeSpinnerProxy;

    ATestTubeSpinners();

    void Tick(float dt) override;

    // Accessors
    auto get_num_instances() const noexcept -> int32;
  protected:
    void BeginPlay() override;

    // Spawning
    void register_all_proxies_in_level();
    void spawn_instances(TConstArrayView<FTransform> const new_transforms);

    // Movement
    void rotate_instances(float const dt);

    // Visuals
    void configure_ismc();
    void update_ismc();

    // Firing
    void fire_lasers();

    // Debugging
    bool array_sizes_consistent() const;

    // Misc
    void clear_runtime_state();

    // Config
    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<UTestTubeSpinnersConfig> actor_config{nullptr};

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;

    // Movement / position
    UPROPERTY()
    TArray<FTransform> transforms{};
    UPROPERTY()
    TArray<float> yaws{};

    // Firing
    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY()
    FCountdownTimers laser_cooldowns;
    UPROPERTY()
    TArray<int32> next_fire_point_indices;
    UPROPERTY()
    TArray<int32> indices_ready_to_fire;
    UPROPERTY()
    TArray<FTransform> new_laser_transforms;

    // Debugging / logging
    UPROPERTY(EditAnywhere, Category = "Turrets")
    FActorLoggingConfig log_config{1.f};
};
