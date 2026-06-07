#pragma once

#include "Sandbox/logging/ActorLoggingConfig.h"
#include "Sandbox/misc/learning/TestEntityOwnerId.h"

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

    static constexpr bool is_world_space{false};

    ATestTubeSpinners();

    void clear_runtime_state();
    void begin_play();
    void tick(float const dt);
    void update_entity_registry();
    void update_visuals();

    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;
  protected:
    // Spawning
    void register_all_proxies_in_level();
    void spawn_instances(TConstArrayView<FTransform> const new_transforms,
                         TConstArrayView<int32> const new_fire_point_indices);

    // Movement
    void rotate_instances(float const dt);

    // Visuals
    void configure_ismc();
    void update_ismc();

    // Firing
    void fire_lasers();

    // Debugging
    bool array_sizes_consistent() const;

    // Config
    UPROPERTY(EditAnywhere, Category = "Turrets")
    TObjectPtr<UTestTubeSpinnersConfig> actor_config{nullptr};

    // Entity data
    TestEntityOwnerId owner_id{};

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
