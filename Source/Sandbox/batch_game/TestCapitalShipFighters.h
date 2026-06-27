#pragma once

#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <SandboxCore/countdown_timers.h>
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
    void move(float const dt);
    void queue_commands();
    void resolve_hit_events();
    void update_entity_registry();
    void sync_from_registry();
    void update_visuals();
    void end_tick();

    void spawn_instances(FVectors3f::ConstView const new_locations,
                         FRotatorsf::ConstView const new_rotations,
                         TConstArrayView<ETestTeam> const new_teams,
                         TConstArrayView<FRegistryEntityHandle> const new_targets);

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
    // Combat
    void handle_firing();

    // Visuals
    void configure_ismc();
    void update_ismc_transforms();
    void draw_debug_shapes();

    // Entity data
    auto get_entity_data() const -> FTestEntityRegistryEntityData;

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

    TArray<FRegistryEntityHandle> entity_handles;
    TArray<int32> local_indices_to_remove;
    EntityDeathInfo entity_death_info;

    // Transform
    FVectors3f locations;
    FVectors3f directions;
    TArray<float> speeds;
    float turn_speed_radians{0.f};
    float turn_speed_unitless{0.5f};

    // Team
    TArray<ETestTeam> teams{};

    // Combat
    TArray<int32> healths;

    // Targets
    TArray<FRegistryEntityHandle> target_indices;
    FVectors3f target_locations;
    FVectors3f target_directions;
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    float fire_dot_product_threshold{0.95f};

    // Laser
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    FCountdownTimers laser_cooldowns;
    FTestLasersSpawnRequests new_lasers;

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debugging")
    bool enable_target_debug_drawing{false};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debugging")
    bool enable_ship_location_debug_drawing{false};
};
