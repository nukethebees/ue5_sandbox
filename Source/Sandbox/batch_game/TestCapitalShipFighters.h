#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/SpatialQueryHit.h>
#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestCapitalShipFighterOrderQueue.h>
#include <Sandbox/batch_game/TestCapitalShipFighterSpawnQueue.h>
#include <Sandbox/batch_game/TestCapitalShipFightersSoA.h>
#include <Sandbox/batch_game/TestCapitalShipFightersTask.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/utilities/DrawDebugConfig.h>
#include <Sandbox/utilities/IndexSpan.h>
#include <SandboxGameShared/utilities/enums.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/tick_countdown.h>

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Containers/StaticArray.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipFighters.generated.h"

class UInstancedStaticMeshComponent;
class UPrimitiveComponent;

class UTestCapitalShipFightersConfig;
class ATestLasers;
struct FTestEntityRegistry;
class ATestBatchOrchestrator;
struct FTestCapitalShipFightersSpatialQueryAccess;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_capital_ship_fighters {
class CommandInterface;
class PhaseInterface;
}

UCLASS()
class SANDBOX_API ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
    friend class ml::test_capital_ship_fighters::CommandInterface;
    friend class ml::test_capital_ship_fighters::PhaseInterface;
  public:
    using RegistryEntityData = ml::entity_registry::EntityData;

    using EntityData = ml::test_capital_ship_fighters::EntityData;
    using EntityBuffers = ml::MultiBuffer<EntityData, 2>;
    using Task = ETestCapitalShipFightersTask;
    static constexpr auto n_task_types{ml::EnumCountTrait<Task>::count_value};
    using TaskSpans = TStaticArray<FIndexSpan, n_task_types>;
    using TaskCounts = TStaticArray<int32, n_task_types>;

    using TaskView = EntityData::View;
    using ConstTaskView = EntityData::ConstView;
    using TaskViews = TStaticArray<TaskView, n_task_types>;
    using ConstTaskViews = TStaticArray<ConstTaskView, n_task_types>;

    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3}; // RGB[3]

    ATestCapitalShipFighters();

    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;
    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_actor_config(UTestCapitalShipFightersConfig* const new_config) noexcept {
        actor_config = new_config;
    }

    auto get_entity_registry() const -> FTestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(FTestEntityRegistry& reg) { entity_registry = &reg; }
    void set_spatial_query_manager(ml::FSpatialQueryManager const& manager) {
        spatial_query_manager = &manager;
    }

    auto get_laser_actor() const -> ATestLasers const* { return laser_actor; }
    void set_laser_actor(ATestLasers& new_ref) { laser_actor = &new_ref; }

    auto get_view(int32 const offset, int32 const width) -> EntityData::View {
        return entity_buffers.current().get_view(offset, width);
    }
    auto get_const_view(int32 const offset, int32 const width) const -> EntityData::ConstView {
        return entity_buffers.current().get_const_view(offset, width);
    }

    auto get_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return entity_buffers.current().entity_handles;
    }

    auto get_locations() const { return entity_buffers.current().locations.get_view(); }

    auto has_handle(FRegistryEntityHandle const fighter_handle) const -> bool {
        return find_index(fighter_handle) != INDEX_NONE;
    }

    auto get_target_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return entity_buffers.current().target_handles;
    }
    auto get_target_handle(FRegistryEntityHandle const fighter_handle) const noexcept
        -> FRegistryEntityHandle {
        return entity_buffers.current().target_handles[find_index(fighter_handle)];
    }

    auto get_target_locations() const {
        return entity_buffers.current().target_locations.get_view();
    }
    auto get_target_location(FRegistryEntityHandle const fighter_handle) const {
        return ml::get_vector3f(entity_buffers.current().target_locations,
                                find_index(fighter_handle));
    }

    auto get_tasks() const -> TConstArrayView<Task> { return entity_buffers.current().tasks; }

    auto get_teams() const -> TConstArrayView<ETestTeam> { return entity_buffers.current().teams; }

// Checks
#if DO_CHECK
    void validate_array_sizes() const;
    void check_fighter_tasks() const;
#else
    void validate_array_sizes() const {}
    void check_fighter_tasks() const {}
#endif
  private:
    void clear_runtime_state();
    void begin_play();
    void begin_tick();
    void update_timers(float const dt);
    void make_decisions();
    void move(float const dt);
    void queue_commands();
    void resolve_damage_events();
    void update_entity_registry();
    void sync_from_registry();
    void update_visual_data();
    void commit_visual_data();
    void end_tick();

    void queue_spawns(TestCapitalShipFighterSpawnQueue const& queue);
    void queue_orders(TestCapitalShipFighterOrderQueue const& queue);
    void self_destruct_fighter(FRegistryEntityHandle handle);

    auto get_new_spawn_entity_data() const -> auto const& { return new_spawn_entity_data; }
    auto get_new_spawn_entity_handles() const -> auto const& { return new_spawn_entity_handles; }

    // Accessors
    auto get_task_view(Task task) noexcept -> TaskView const&;
    auto get_const_task_view(Task task) const noexcept -> ConstTaskView const&;

    auto set_target_handle_unchecked(int32 const fighter_idx,
                                     FRegistryEntityHandle const new_target) noexcept {
        entity_buffers.current().target_handles[fighter_idx] = new_target;
    }
    auto set_target_handle(FRegistryEntityHandle const fighter_handle,
                           FRegistryEntityHandle const new_target) noexcept {
        auto const idx{find_index(fighter_handle)};
        set_target_handle_unchecked(idx, new_target);
    }

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

    // Movement
    void move(float const dt, TaskView const& task_span);

    // Combat
    void handle_firing(TaskView const& data);

    // Spawning
    void commit_spawns();

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

    // Orders
    void refresh_task_views();
    void commit_orders();

    // Targets
    void refresh_target_data();

    // Misc
    void clear_tick_buffers();
    void remove_dead_entities();

    auto get_spatial_query_component() const -> UPrimitiveComponent const*;
    void resolve_hits(TConstArrayView<ml::FSpatialQueryHit> hits,
                      TArrayView<FRegistryEntityHandle> out_entity_handles) const;
    void visual_log_state() const;

    // Visuals
    UPROPERTY(EditDefaultsOnly, meta = (AllowPrivateAccess))
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    TArray<FTransform> ismc_transforms;

    // Config data
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<UTestCapitalShipFightersConfig> actor_config{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    FTickCountdown16::counter_type attack_retry_cooldown_tick_value{0};

    // Entity data
    EntityBuffers entity_buffers{};

    FTestEntityRegistry* entity_registry{nullptr};
    ml::FSpatialQueryManager const* spatial_query_manager{nullptr};
    RegistryEntityData registry_update_data;

    // Spawning
    TestCapitalShipFighterSpawnQueue spawn_queue;
    RegistryEntityData new_spawn_entity_data;
    SpawnedEntityHandles new_spawn_entity_handles;
    TArray<float> custom_data_buffer;

    // Destruction
    TArray<int32> local_indices_to_remove;
    EntityDeathInfo entity_death_info;

    // Tasks
    TaskSpans task_spans{};
    TaskViews task_views{};
    ConstTaskViews const_task_views{};
    TestCapitalShipFighterOrderQueue order_queue{};

    // Targets
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    float fire_dot_product_threshold{0.95f};

    // Laser
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    ml::test_lasers::SpawnRequests new_lasers;
    TArray<float> aiming_dot_product_buffer;

    // Misc buffers
    TArray<int32> scratch_int_buffer;

    // Debugging
    UPROPERTY(EditAnywhere, meta = (AllowPrivateAccess))
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debugging", meta = (AllowPrivateAccess))
    bool enable_target_debug_drawing{false};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debugging", meta = (AllowPrivateAccess))
    bool enable_ship_location_debug_drawing{false};

    friend struct FTestCapitalShipFightersSpatialQueryAccess;
};

struct FTestCapitalShipFightersSpatialQueryAccess {
    ATestCapitalShipFighters const* actor{nullptr};

    auto get_spatial_query_component() const -> UPrimitiveComponent const* {
        return actor->get_spatial_query_component();
    }

    void resolve_hits(TConstArrayView<ml::FSpatialQueryHit> const hits,
                      TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
        actor->resolve_hits(hits, out_entity_handles);
    }
};
