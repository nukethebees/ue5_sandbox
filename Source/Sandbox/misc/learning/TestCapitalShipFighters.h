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
    void tick(float const dt);
    void update_entity_registry();
    void resolve_damage_targets();
    void sync_from_registry();
    void update_visuals();
    void end_frame();

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

    // Visuals
    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY()
    TArray<FTransform> ismc_transforms;

    // Config data
    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<UTestCapitalShipFightersConfig> actor_config{nullptr};

    // Entity data
    TestEntityOwnerId owner_id{};

    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
    UPROPERTY()
    TArray<FGenerationIndex> entity_indices;
    UPROPERTY()
    TArray<int32> local_indices_to_remove;

    // Transform
    UPROPERTY()
    FVectors3f locations;
    UPROPERTY()
    FRotatorsf rotations;
    UPROPERTY()
    FVectors3f velocities;

    // Team
    UPROPERTY()
    TArray<ETestTeam> teams{};

    // Combat
    UPROPERTY()
    TArray<int32> healths;
    UPROPERTY()
    TArray<FGenerationIndex> target_indices{};

    // Laser
    UPROPERTY(EditAnywhere, Category = "Ships")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY()
    FCountdownTimers laser_cooldowns;
    UPROPERTY()
    TArray<int32> indices_ready_to_fire_buffer;
    UPROPERTY()
    FVectors3f new_laser_locations;
    UPROPERTY()
    FRotatorsf new_laser_rotations;
};
