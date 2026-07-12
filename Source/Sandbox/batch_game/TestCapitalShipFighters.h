#pragma once

#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <SandboxCore/countdown_timers.h>
#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipFighters.generated.h"

class UInstancedStaticMeshComponent;

class UTestCapitalShipFightersConfig;
class ATestLasers;
class ATestEntityRegistry;

struct FTestCapitalShipFightersEntityData : public ml::FSoAArrayMixin {
    FVectors3f locations;
    FVectors3f directions;
    TArray<float> speeds;

    TArray<ETestTeam> teams{};
    TArray<int32> healths;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(
            self.locations, self.directions, self.speeds, self.teams, self.healths);
    }
};

UCLASS()
class SANDBOX_API ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
  public:
    using EntityBuffers = ml::MultiBuffer<FTestCapitalShipFightersEntityData, 2>;

    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3}; // RGB[3]

    ATestCapitalShipFighters();

    void clear_runtime_state();
    void begin_play();

    void begin_tick();
    void update_timers(float const dt);
    void make_decisions();
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

    void self_destruct_fighter(FRegistryEntityHandle handle);

    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    auto get_entity_registry() const -> ATestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(ATestEntityRegistry& reg) { entity_registry = &reg; }

    auto get_laser_actor() const -> ATestLasers const* { return laser_actor; }
    void set_laser_actor(ATestLasers& new_ref) { laser_actor = &new_ref; }

    auto get_new_spawn_entity_data() const -> auto const& { return new_spawn_entity_data; }
    auto get_new_spawn_entity_handles() const -> auto const& { return new_spawn_entity_handles; }

    auto get_target_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return target_handles;
    }
    auto set_target_handle(int32 const fighter_idx,
                           FRegistryEntityHandle const new_target) noexcept {
        target_handles[fighter_idx] = new_target;
    }

    // Checks
    void validate_array_sizes() const;
  protected:
    // Combat
    void handle_firing();

    // Visuals
    void configure_ismc();
    void prepare_ismc_transforms();
    void draw_debug_shapes();
    void update_ismc();
    void write_ismc_custom_data(int32 offset, int32 count);
    void write_ismc_custom_data();

    // Entity data
    void prepare_entity_update_data();

    // Misc
    void clear_tick_buffers();
    void remove_dead_entities();

    // Visuals
    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    TArray<FTransform> ismc_transforms;

    // Config data
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestCapitalShipFightersConfig> actor_config{nullptr};

    // Entity data
    TestEntityOwnerId owner_id{};
    EntityBuffers entity_buffers{};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    TArray<FRegistryEntityHandle> entity_handles;
    FTestEntityRegistryEntityData registry_update_data;

    // Spawning / destruction
    TArray<int32> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    FTestEntityRegistryEntityData new_spawn_entity_data;
    SpawnedEntityHandles new_spawn_entity_handles;
    TArray<float> custom_data_buffer;

    // Transform
    float turn_speed_radians{0.f};
    float turn_speed_unitless{0.5f};

    // Targets
    TArray<FRegistryEntityHandle> target_handles;
    FVectors3f target_locations;
    FVectors3f target_directions;
    TArray<float> target_distance_sq;

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
