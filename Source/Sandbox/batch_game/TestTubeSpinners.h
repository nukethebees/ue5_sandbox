#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/logging/ActorLoggingConfig.h>

#include <SandboxCore/countdown_timers.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestTubeSpinners.generated.h"

class UInstancedStaticMeshComponent;

class ATestTubeSpinnerProxy;
class UTestTubeSpinnersConfig;
class ATestLasers;
class ATestEntityRegistry;

UCLASS()
class ATestTubeSpinners : public AActor {
    GENERATED_BODY()
  public:
    using Proxy = ATestTubeSpinnerProxy;

    static constexpr bool is_world_space{false};

    ATestTubeSpinners();

    void clear_runtime_state();
    void begin_play();

    void begin_tick();
    void update_timers(float const dt);
    void move(float const dt);
    void queue_commands();
    void update_entity_registry();
    void update_visuals();
    void end_tick();

    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    auto get_entity_registry() const -> ATestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(ATestEntityRegistry& reg) { entity_registry = &reg; }

    auto get_laser_actor() const -> ATestLasers const* { return laser_actor; }
    void set_laser_actor(ATestLasers& new_ref) { laser_actor = &new_ref; }

    // Checks
    void validate_array_sizes() const;
  protected:
    // Spawning
    void register_all_proxies_in_level();
    void spawn_instances(FVectors3f::ConstView const new_locations,
                         TConstArrayView<float> const new_yaws,
                         TConstArrayView<int32> const new_fire_point_indices);

    // Movement
    void rotate_instances(float const dt);

    // Visuals
    void configure_ismc();
    void update_ismc_transforms();
    void update_ismc();

    // Firing
    void fire_lasers();

    // Config
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestTubeSpinnersConfig> actor_config{nullptr};

    // Entity data
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    TestEntityOwnerId owner_id{};
    TArray<FRegistryEntityHandle> registry_entity_handles;

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY()
    TArray<FTransform> ismc_transforms;

    // Movement / position
    UPROPERTY()
    FVectors3f locations;
    UPROPERTY()
    TArray<float> yaws{};

    // Firing
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    FCountdownTimers laser_cooldowns;
    TArray<int32> next_fire_point_indices;
    TArray<int32> indices_ready_to_fire;
    FTestLasersSpawnRequests new_lasers;

    // Debugging / logging
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    FActorLoggingConfig log_config{1.f};
};
