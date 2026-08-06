#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestCapitalShipFighterOrderQueue.h>
#include <Sandbox/batch_game/TestCapitalShipFighterSpawnQueue.h>
#include <Sandbox/batch_game/TestCapitalShipFightersTask.h>
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
#include <SandboxCore/tick_countdown.h>

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Containers/StaticArray.h"
#include "GameFramework/Actor.h"

#include <type_traits>

#include "TestCapitalShipFighters.generated.h"

class UInstancedStaticMeshComponent;

class UTestCapitalShipFightersConfig;
class ATestLasers;
class ATestEntityRegistry;
class ATestBatchOrchestrator;

namespace ml::test_capital_ship_fighters {
class CommandInterface;

template <bool is_const>
struct EntityDataView : public ml::FSoAViewMixin {
    using View = EntityDataView<false>;
    using ConstView = EntityDataView<true>;

    template <typename T>
    using TView = std::conditional_t<is_const, TConstArrayView<T>, TArrayView<T>>;
    using VectorsView = std::conditional_t<is_const, FVectors3f::ConstView, FVectors3f::View>;

#define EXPAND(X) X
#define EXPAND_WITH_COMMA(X) X,
#define MEMBER_DECL(TYPE, NAME) TYPE NAME;
#define FN_ARG(TYPE, NAME) self.NAME

#define SANDBOX_CLASS_MEMBERS(X, NON_FINAL)                                         \
    NON_FINAL(X(TView<FRegistryEntityHandle>, entity_handles))                      \
    NON_FINAL(X(TView<ETestCapitalShipFightersTask>, tasks))                        \
    NON_FINAL(X(VectorsView, locations))                                            \
    NON_FINAL(X(VectorsView, aim_directions))                                       \
    NON_FINAL(X(VectorsView, move_target_locations))                                \
    NON_FINAL(X(VectorsView, movement_directions))                                  \
    NON_FINAL(X(TView<float>, move_distances))                                      \
    NON_FINAL(X(TView<float>, speeds))                                              \
    NON_FINAL(X(TView<ETestTeam>, teams))                                           \
    NON_FINAL(X(TView<int32>, healths))                                             \
    NON_FINAL(X(TView<FTickCountdown::counter_type>, awareness_scan_countdowns))    \
    NON_FINAL(X(TView<FTickCountdown::counter_type>, attack_reposition_countdowns)) \
    NON_FINAL(X(TView<float>, attack_cooldowns))                                    \
    NON_FINAL(X(TView<FRegistryEntityHandle>, target_handles))                      \
    NON_FINAL(X(VectorsView, target_locations))                                     \
    NON_FINAL(X(VectorsView, target_velocities))                                    \
    NON_FINAL(X(VectorsView, target_directions))                                    \
    NON_FINAL(X(TView<float>, intercept_times))                                     \
    NON_FINAL(X(VectorsView, desired_firing_directions))                            \
    NON_FINAL(X(TView<float>, target_distance_sq))                                  \
    NON_FINAL(X(TView<float>, target_distances))                                    \
    X(TView<float>, target_radii)

    SANDBOX_CLASS_MEMBERS(MEMBER_DECL, EXPAND)

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(SANDBOX_CLASS_MEMBERS(FN_ARG, EXPAND_WITH_COMMA));
    }

#undef EXPAND
#undef EXPAND_WITH_COMMA
#undef MEMBER_DECL
#undef FN_ARG
#undef SANDBOX_CLASS_MEMBERS
};

struct EntityData : public ml::FSoAArrayMixin {
    using View = EntityDataView<false>;
    using ConstView = EntityDataView<true>;

    TArray<FRegistryEntityHandle> entity_handles;
    TArray<ETestCapitalShipFightersTask> tasks;
    FVectors3f locations;
    FVectors3f aim_directions;
    FVectors3f move_target_locations;
    FVectors3f movement_directions;
    TArray<float> move_distances;
    TArray<float> speeds;
    TArray<ETestTeam> teams{};
    TArray<int32> healths;
    FTickCountdown awareness_scan_countdowns;
    FTickCountdown attack_reposition_countdowns;
    FCountdownTimers attack_cooldowns;

    TArray<FRegistryEntityHandle> target_handles;
    FVectors3f target_locations;
    FVectors3f target_velocities;
    FVectors3f target_directions;
    TArray<float> intercept_times;
    FVectors3f desired_firing_directions;
    TArray<float> target_distance_sq;
    TArray<float> target_distances;
    TArray<float> target_radii;

#define SANDBOX_PACK(STAMPER, NON_FINAL)             \
    NON_FINAL(STAMPER(entity_handles))               \
    NON_FINAL(STAMPER(tasks))                        \
    NON_FINAL(STAMPER(locations))                    \
    NON_FINAL(STAMPER(aim_directions))               \
    NON_FINAL(STAMPER(move_target_locations))        \
    NON_FINAL(STAMPER(movement_directions))          \
    NON_FINAL(STAMPER(move_distances))               \
    NON_FINAL(STAMPER(speeds))                       \
    NON_FINAL(STAMPER(teams))                        \
    NON_FINAL(STAMPER(healths))                      \
    NON_FINAL(STAMPER(awareness_scan_countdowns))    \
    NON_FINAL(STAMPER(attack_reposition_countdowns)) \
    NON_FINAL(STAMPER(attack_cooldowns))             \
    NON_FINAL(STAMPER(target_handles))               \
    NON_FINAL(STAMPER(target_locations))             \
    NON_FINAL(STAMPER(target_velocities))            \
    NON_FINAL(STAMPER(target_directions))            \
    NON_FINAL(STAMPER(intercept_times))              \
    NON_FINAL(STAMPER(desired_firing_directions))    \
    NON_FINAL(STAMPER(target_distance_sq))           \
    NON_FINAL(STAMPER(target_distances))             \
    STAMPER(target_radii)

    SANDBOX_SOA_MAKE_APPLY_FNS(SANDBOX_PACK)
#undef SANDBOX_PACK
};
}

UCLASS()
class SANDBOX_API ATestCapitalShipFighters : public AActor {
    GENERATED_BODY()
    friend class ml::test_capital_ship_fighters::CommandInterface;
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

    void clear_runtime_state();
    void begin_play();

    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;

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

    // Accessors
    auto get_num_instances() const noexcept -> int32;

    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    void set_actor_config(UTestCapitalShipFightersConfig* const new_config) noexcept {
        actor_config = new_config;
    }

    auto get_entity_registry() const -> ATestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(ATestEntityRegistry& reg) { entity_registry = &reg; }

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
  protected:
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

    // Visuals
    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    TArray<FTransform> ismc_transforms;

    // Config data
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestCapitalShipFightersConfig> actor_config{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;

    // Entity data
    TestEntityOwnerId owner_id{};
    EntityBuffers entity_buffers{};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
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
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    float fire_dot_product_threshold{0.95f};

    // Laser
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    ml::test_lasers::SpawnRequests new_lasers;
    TArray<float> aiming_dot_product_buffer;

    // Misc buffers
    TArray<int32> scratch_int_buffer;

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debugging")
    bool enable_target_debug_drawing{false};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debugging")
    bool enable_ship_location_debug_drawing{false};
};

namespace ml::test_capital_ship_fighters {
class SANDBOX_API CommandInterface {
  public:
    inline void bind(ATestCapitalShipFighters& new_fighters) { fighters = &new_fighters; }

    inline void queue_spawns(TestCapitalShipFighterSpawnQueue const& queue) {
        check(IsValid(fighters));
        fighters->queue_spawns(queue);
    }

    inline void queue_orders(TestCapitalShipFighterOrderQueue const& queue) {
        check(IsValid(fighters));
        fighters->queue_orders(queue);
    }

    inline void self_destruct_fighter(FRegistryEntityHandle const handle) {
        check(IsValid(fighters));
        fighters->self_destruct_fighter(handle);
    }

    inline auto get_new_spawn_entity_data() const
        -> ATestCapitalShipFighters::RegistryEntityData const& {
        check(IsValid(fighters));
        return fighters->get_new_spawn_entity_data();
    }

    inline auto get_new_spawn_entity_handles() const -> SpawnedEntityHandles const& {
        check(IsValid(fighters));
        return fighters->get_new_spawn_entity_handles();
    }

    inline auto get_num_instances() const noexcept -> int32 {
        check(IsValid(fighters));
        return fighters->get_num_instances();
    }

    inline auto get_target_handle(FRegistryEntityHandle const fighter_handle) const noexcept
        -> FRegistryEntityHandle {
        check(IsValid(fighters));
        return fighters->get_target_handle(fighter_handle);
    }
  private:
    ATestCapitalShipFighters* fighters{nullptr};
};
}
