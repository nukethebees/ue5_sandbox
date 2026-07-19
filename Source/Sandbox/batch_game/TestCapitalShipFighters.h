#pragma once

#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/utilities/DrawDebugConfig.h>
#include <Sandbox/utilities/enums.h>
#include <Sandbox/utilities/IndexSpan.h>

#include <SandboxCore/countdown_timers.h>
#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipFighters.generated.h"

class UInstancedStaticMeshComponent;

class UTestCapitalShipFightersConfig;
class ATestLasers;
class ATestEntityRegistry;

UENUM()
enum class ETestCapitalShipFightersTask : uint8 {
    Standby,
    MoveToDestination,
    Attack,
    COUNT UMETA(DisplayName = "Count", Hidden),
};

inline auto LexToString(ETestCapitalShipFightersTask const task) -> FString {
    return ml::to_string_without_type_prefix(task);
}

struct FTestCapitalShipFightersEntityData : public ml::FSoAArrayMixin {
    TArray<FRegistryEntityHandle> entity_handles;

    TArray<ETestCapitalShipFightersTask> tasks;

    FVectors3f locations;
    FVectors3f directions;
    TArray<float> speeds;

    TArray<ETestTeam> teams{};
    TArray<int32> healths;

    TArray<FRegistryEntityHandle> target_handles;

    FCountdownTimers laser_cooldowns;

    // clang-format off
#define SANDBOX_PACK(STAMPER, END_SYMBOL)  \
    STAMPER(entity_handles)    \
    END_SYMBOL STAMPER(tasks)           \
    END_SYMBOL STAMPER(locations)       \
    END_SYMBOL STAMPER(directions)      \
    END_SYMBOL STAMPER(speeds)          \
    END_SYMBOL STAMPER(teams)           \
    END_SYMBOL STAMPER(healths)         \
    END_SYMBOL STAMPER(target_handles)  \
    END_SYMBOL STAMPER(laser_cooldowns)
    // clang-format on

    SANDBOX_SOA_MAKE_APPLY_FNS(SANDBOX_PACK)
#undef SANDBOX_PACK
};

UCLASS()
class SANDBOX_API ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
  public:
    using EntityBuffers = ml::MultiBuffer<FTestCapitalShipFightersEntityData, 2>;
    using Task = ETestCapitalShipFightersTask;
    static constexpr auto n_task_types{ml::EnumCountTrait<Task>::count_value};
    using TaskSpans = TStaticArray<FIndexSpan, n_task_types>;
    using TaskCounts = TStaticArray<int32, n_task_types>;

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

    auto get_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return entity_buffers.current().entity_handles;
    }

    auto get_target_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return entity_buffers.current().target_handles;
    }
    auto get_target_handle(FRegistryEntityHandle const fighter_handle) const noexcept
        -> FRegistryEntityHandle {
        return entity_buffers.current().target_handles[find_index(fighter_handle)];
    }

    auto set_target_handle_unchecked(int32 const fighter_idx,
                                     FRegistryEntityHandle const new_target) noexcept {
        entity_buffers.current().target_handles[fighter_idx] = new_target;
    }
    auto set_target_handle(FRegistryEntityHandle const fighter_handle,
                           FRegistryEntityHandle const new_target) noexcept {
        auto const idx{find_index(fighter_handle)};
        set_target_handle_unchecked(idx, new_target);
    }

    auto get_target_locations() const { return target_locations.get_view(); }
    auto get_target_location(FRegistryEntityHandle const fighter_handle) const {
        return ml::get_vector3f(target_locations, find_index(fighter_handle));
    }

    auto get_tasks() const -> TConstArrayView<Task> { return entity_buffers.current().tasks; }
    void set_task_unchecked(int32 const i, Task const task) noexcept {
        entity_buffers.current().tasks[i] = task;
    }
    void set_task(FRegistryEntityHandle const handle, Task const task) noexcept {
        set_task_unchecked(find_index(handle), task);
    }

    auto find_index(FRegistryEntityHandle const fighter_handle) const noexcept -> int32 {
        return entity_buffers.current().entity_handles.Find(fighter_handle);
    }

    // It is an error to call this when spans are invalid
    auto get_task_spans() const -> TaskSpans;
    auto get_task_span(Task const task) const -> FIndexSpan {
        return task_spans[std::to_underlying(task)];
    }
    auto get_task_counts() const -> TaskCounts;

// Checks
#if DO_CHECK
    void validate_array_sizes() const;
    void check_fighter_tasks() const;
#else
    void validate_array_sizes() const {}
    void check_fighter_tasks() const {}
#endif
  protected:
    // Movement
    void move(float const dt, FIndexSpan task_span);

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
    bool tasks_are_contiguous() const noexcept;
    void refresh_layout();

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

    // Taks
    TaskSpans task_spans{};

    // Targets
    FVectors3f target_locations;
    FVectors3f target_directions;
    TArray<float> target_distance_sq;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    float fire_dot_product_threshold{0.95f};

    // Laser
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    FTestLasersSpawnRequests new_lasers;

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debugging")
    bool enable_target_debug_drawing{false};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debugging")
    bool enable_ship_location_debug_drawing{false};
};
