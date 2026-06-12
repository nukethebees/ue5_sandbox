#pragma once

#include "Sandbox/misc/learning/TestEntityOwnerId.h"
#include "Sandbox/misc/learning/TestEntityRegistry.h"
#include "Sandbox/misc/learning/TestEntityRegistryData.h"
#include "Sandbox/misc/learning/TestTeam.h"

#include <SandboxCore/countdown_timers.h>
#include <SandboxCore/generation_index.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipFighters.generated.h"

class UInstancedStaticMeshComponent;

class UTestCapitalShipFightersConfig;
class ATestLasers;
class ATestEntityRegistry;

UCLASS()
class ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
  public:
    static constexpr bool is_world_space{false};

    ATestCapitalShipFighters();

    void clear_runtime_state();
    void begin_play();

    void begin_tick();
    void tick(float const dt);
    void update_entity_registry();
    void resolve_damage_targets();
    void sync_from_registry();
    void update_visuals();
    void end_tick();

    void spawn_instances(FVectors3f::ConstView const new_locations,
                         FRotatorsf::ConstView const new_rotations,
                         TConstArrayView<ETestTeam> const new_teams,
                         TConstArrayView<FGenerationIndex> const new_targets);

    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    // Checks
    void validate_array_sizes() const;
  protected:
    // Movement
    void move_ships(float const dt);

    // Combat
    void handle_firing();

    // Visuals
    void configure_ismc();
    void update_ismc_transforms();

    // Entity data
    auto get_entity_data(int32 const offset, int32 const count) const
        -> FTestEntityRegistryEntityData;

    // Misc
    void clear_tick_buffers();

    // Visuals
    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY()
    TArray<FTransform> ismc_transforms;

    // Config data
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestCapitalShipFightersConfig> actor_config{nullptr};

    // Entity data
    TestEntityOwnerId owner_id{};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
    TArray<FGenerationIndex> entity_indices;
    TArray<int32> local_indices_to_remove;

    // Transform
    FVectors3f locations;
    FRotatorsf rotations;
    FVectors3f velocities;

    // Team
    TArray<ETestTeam> teams{};

    // Combat
    TArray<int32> healths;

    // Targets
    TArray<FGenerationIndex> target_indices;
    FVectors3f target_locations;

    // Laser
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    FCountdownTimers laser_cooldowns;
    TArray<int32> indices_ready_to_fire_buffer;
    FVectors3f new_laser_locations;
    FRotatorsf new_laser_rotations;
};
