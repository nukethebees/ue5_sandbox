#pragma once

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/combat/lasers/TestLasers.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersSoA.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>
#include <SpaceGame/support/logging/ActorLoggingConfig.h>

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
class ATestLasers;
struct FTestEntityRegistry;
class ATestBatchOrchestrator;

namespace ml::test_tube_spinners {
class PhaseInterface;
}

UCLASS()
class ATestTubeSpinners : public AActor {
    GENERATED_BODY()
    friend class ml::test_tube_spinners::PhaseInterface;
  public:
    using Proxy = ATestTubeSpinnerProxy;
    using EntityData = ml::test_tube_spinners::EntityData;

    static constexpr bool is_world_space{false};

    ATestTubeSpinners();

    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;
    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_actor_config(FTubeSpinnerConfig const* const new_config) noexcept {
        actor_config = new_config;
    }

    auto get_entity_registry() const -> FTestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(FTestEntityRegistry& reg) { entity_registry = &reg; }

    auto get_laser_actor() const -> ATestLasers const* { return laser_actor; }
    void set_laser_actor(ATestLasers& new_ref) { laser_actor = &new_ref; }

    // Checks
    void validate_array_sizes() const;
  private:
    void clear_runtime_state();
    void begin_play();
    void begin_tick();
    void update_timers(float const dt);
    void move(float const dt);
    void queue_commands();
    void update_entity_registry();
    void update_visual_data();
    void commit_visual_data();
    void end_tick();

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
    FTubeSpinnerConfig const* actor_config{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;

    // Entity data
    FTestEntityRegistry* entity_registry{nullptr};

    EntityData entities{};

    // Visuals
    UPROPERTY(meta = (AllowPrivateAccess))
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY(meta = (AllowPrivateAccess))
    TArray<FTransform> ismc_transforms;

    // Firing
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    TArray<int32> indices_ready_to_fire;
    ml::test_lasers::SpawnRequests new_lasers;
};
