#pragma once

#include <SandboxGameShared/utilities/enums.h>
#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/entities/EntityDeathInfo.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityRegistryData.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighterOrderQueue.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighterSpawnQueue.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSoA.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersTask.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>
#include <SpaceGame/support/IndexSpan.h>

#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/tick_countdown.h>

#include <Containers/ArrayView.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

class ATestBatchOrchestrator;
class ATestCapitalShipFighters;
struct FFighterConfig;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_capital_ship_fighters {
class CommandInterface;
class PhaseInterface;

struct SPACEGAME_API Simulation {
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

    void set_config(FFighterConfig const& new_config) noexcept;
    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;
    void set_entity_registry(FTestEntityRegistry& new_entity_registry) noexcept;
    void set_spatial_query_manager(FSpatialQueryManager const& new_query_manager) noexcept;
    void set_laser_simulation(ml::test_lasers::Simulation& new_simulation) noexcept;

    auto get_num_instances() const noexcept -> int32;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const* {
        return entity_registry;
    }
    auto get_laser_simulation() const noexcept -> ml::test_lasers::Simulation const* {
        return laser_simulation;
    }
    auto get_view(int32 offset, int32 width) -> EntityData::View;
    auto get_const_view(int32 offset, int32 width) const -> EntityData::ConstView;
    auto get_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle>;
    auto get_locations() const { return entity_buffers.current().locations.get_view(); }
    auto has_handle(FRegistryEntityHandle fighter_handle) const -> bool;
    auto get_target_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle>;
    auto get_target_handle(FRegistryEntityHandle fighter_handle) const noexcept
        -> FRegistryEntityHandle;
    auto get_target_locations() const {
        return entity_buffers.current().target_locations.get_view();
    }
    auto get_target_location(FRegistryEntityHandle fighter_handle) const -> FVector3f;
    auto get_tasks() const -> TConstArrayView<Task>;
    auto get_teams() const -> TConstArrayView<ETestTeam>;

#if DO_CHECK
    void validate_array_sizes() const;
    void check_fighter_tasks() const;
#else
    void validate_array_sizes() const {}
    void check_fighter_tasks() const {}
#endif

    float collision_radius{0.f};
    float fire_point_distance{0.f};
    float fire_dot_product_threshold{0.95f};
  private:
    void clear_runtime_state();
    void begin_play();
    void begin_tick();
    void update_timers(float dt);
    void make_decisions();
    void move(float dt);
    void queue_commands();
    void resolve_damage_events();
    void update_entity_registry();
    void sync_from_registry();
    void end_tick();

    void queue_spawns(TestCapitalShipFighterSpawnQueue const& queue);
    void queue_orders(TestCapitalShipFighterOrderQueue const& queue);
    void self_destruct_fighter(FRegistryEntityHandle handle);
    auto get_new_spawn_entity_data() const -> auto const& { return new_spawn_entity_data; }
    auto get_new_spawn_entity_handles() const -> auto const& { return new_spawn_entity_handles; }

    auto get_task_view(Task task) noexcept -> TaskView const&;
    auto get_const_task_view(Task task) const noexcept -> ConstTaskView const&;
    void set_target_handle_unchecked(int32 fighter_index,
                                     FRegistryEntityHandle new_target) noexcept;
    void set_target_handle(FRegistryEntityHandle fighter_handle,
                           FRegistryEntityHandle new_target) noexcept;
    void set_task_unchecked(int32 index, Task task) noexcept;
    void set_task(FRegistryEntityHandle handle, Task task) noexcept;
    auto find_index(FRegistryEntityHandle fighter_handle) const noexcept -> int32;
    auto get_task_spans() const -> TaskSpans;
    auto get_task_span(Task task) const -> FIndexSpan;
    auto get_task_counts() const -> TaskCounts;

    void move(float dt, TaskView const& task_span);
    void handle_firing(TaskView const& data);
    void commit_spawns();
    void prepare_entity_update_data();
    bool tasks_are_contiguous() const noexcept;
    void refresh_layout();
    void refresh_task_views();
    void commit_orders();
    void refresh_target_data();
    void clear_tick_buffers();
    void remove_dead_entities();
    void clear_presentation_events();

    friend class CommandInterface;
    friend class PhaseInterface;
    friend class ::ATestCapitalShipFighters;

    FFighterConfig const* config{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    FTickCountdown16::counter_type attack_retry_cooldown_tick_value{0};

    EntityBuffers entity_buffers{};
    FTestEntityRegistry* entity_registry{nullptr};
    FSpatialQueryManager const* spatial_query_manager{nullptr};
    RegistryEntityData registry_update_data;

    TestCapitalShipFighterSpawnQueue spawn_queue;
    RegistryEntityData new_spawn_entity_data;
    SpawnedEntityHandles new_spawn_entity_handles;

    TArray<int32> local_indices_to_remove;
    EntityDeathInfo entity_death_info;

    TaskSpans task_spans{};
    TaskViews task_views{};
    ConstTaskViews const_task_views{};
    TestCapitalShipFighterOrderQueue order_queue{};

    ml::test_lasers::Simulation* laser_simulation{nullptr};
    ml::test_lasers::SpawnRequests new_lasers;
    TArray<float> aiming_dot_product_buffer;

    TArray<int32> scratch_int_buffer;
    FVectors3f line_of_sight_starts;
    FVectors3f line_of_sight_ends;
    TArray<uint8> line_of_sight_results;

    TArray<int32> presentation_indices_to_remove;
    int32 presentation_spawn_offset{0};
    int32 presentation_spawn_count{0};
};
} // namespace ml::test_capital_ship_fighters
