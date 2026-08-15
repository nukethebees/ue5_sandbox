#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestTubeSpinnersSoA.h>
#include <Sandbox/logging/ActorLoggingConfig.h>

#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>
#include <SandboxCore/tick_countdown.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <utility>

#include "TestTubeSpinners.generated.h"

class UInstancedStaticMeshComponent;

class ATestTubeSpinnerProxy;
class UTestTubeSpinnersConfig;
class ATestLasers;
class ATestEntityRegistry;
class ATestBatchOrchestrator;

UCLASS()
class ATestTubeSpinners : public AActor {
    GENERATED_BODY()
  public:
    using Proxy = ATestTubeSpinnerProxy;
    using EntityData = ml::test_tube_spinners::EntityData;

    static constexpr bool is_world_space{false};

    ATestTubeSpinners();

    void clear_runtime_state();
    void begin_play();
    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;

    void begin_tick();
    void update_timers(float const dt);
    void move(float const dt);
    void queue_commands();
    void update_entity_registry();
    void update_visual_data();
    void commit_visual_data();
    void end_tick();

    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    void set_actor_config(UTestTubeSpinnersConfig* const new_config) noexcept {
        actor_config = new_config;
    }

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
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;

    // Entity data
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    TestEntityOwnerId owner_id{};
    EntityData entities{};

    // Visuals
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY()
    TArray<FTransform> ismc_transforms;

    // Firing
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    TArray<int32> indices_ready_to_fire;
    ml::test_lasers::SpawnRequests new_lasers;
};
