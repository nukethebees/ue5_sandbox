#pragma once
#include <sandbox/simulation/fighter_firing_position.h>
#include <sandbox/simulation/fighter_navigation.h>
#include <sandbox/simulation/fighter_navigation_scratch.h>
#include <sandbox/simulation/fighter_navigation_state.h>
#include <sandbox/simulation/fighter_task_layout.h>
#include <sandbox/simulation/navigation_telemetry.h>

#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>

#include <SandboxCoreEngine/enums.h>
#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/entities/EntityDeathInfo.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/entities/TestEntityRegistryData.h>
#include <SpaceGameSimulation/entities/TestTeamUtils.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFighterOrderQueue.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFighterSpawnQueue.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSoA.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersTask.h>
#include <SpaceGameSimulation/simulation/SimulationClockInterface.h>
#include <SpaceGameSimulation/simulation/TraceHits.h>
#include <SpaceGameSimulation/support/IndexSpan.h>

#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/tick_countdown.h>

#include <Containers/ArrayView.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

#include <array>
#include <memory_resource>

struct FLevelSimulation;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_capital_ship_fighters {
class CommandInterface;
class PhaseInterface;

using FNavigationTelemetrySnapshot = simulation::fighters::NavigationTelemetrySnapshot;

struct SPACEGAMESIMULATION_API Simulation {
    using RegistryEntityData = ml::entity_registry::EntityData;
    using EntityData = ml::test_capital_ship_fighters::EntityData;
    using EntityBuffers = ml::MultiBuffer<EntityData, 2>;
    using Task = ETestCapitalShipFightersTask;
    static constexpr auto n_task_types{ml::EnumCountTrait<Task>::count_value};
    using TaskSpans = ml::simulation::fighters::TaskSpans;
    using TaskCounts = ml::simulation::fighters::TaskCounts;
    using TaskView = EntityData::View;
    using ConstTaskView = EntityData::ConstView;
    using TaskViews = TStaticArray<TaskView, n_task_types>;
    using ConstTaskViews = TStaticArray<ConstTaskView, n_task_types>;

    Simulation(FSimulationClock const& clock,
               FTestEntityRegistry& entity_registry,
               FSpatialQueryManager const& spatial_query_manager,
               ml::test_lasers::Simulation& laser_simulation,
               std::pmr::memory_resource& frame_memory_resource) noexcept;
    Simulation(Simulation const&) = delete;
    Simulation(Simulation&&) = delete;
    auto operator=(Simulation const&) -> Simulation& = delete;
    auto operator=(Simulation&&) -> Simulation& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> FFighterReadView {
        return {entity_buffers.current().get_const_view(), &entity_registry};
    }
    void set_config(FFighterSimulationConfig const& new_config,
                    TConstArrayView<ETestTeam> participating_teams) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> int32;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const& {
        return entity_registry;
    }
    auto get_laser_simulation() const noexcept -> ml::test_lasers::Simulation const& {
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
    auto get_navigation_telemetry() const noexcept -> FNavigationTelemetrySnapshot const& {
        return navigation_telemetry;
    }

    /* **************************************** */
    // Checks
    /* **************************************** */
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
    using FirePointCandidate = ml::simulation::fighters::FirePointCandidate;
    using NavigationScratch = ml::simulation::fighters::NavigationScratch;
    using NavigationRiskTier = ml::simulation::fighters::NavigationRiskTier;

    inline static constexpr int8 direct_movement_choice{-1};
    inline static constexpr int8 stop_movement_choice{-2};
    inline static constexpr int32 n_avoidance_choices{
        ml::simulation::fighters::avoidance_direction_count};
    inline static constexpr uint8 clear_scans_to_end_avoidance{2};
    inline static constexpr uint8 lower_risk_scans_to_demote{2};
    inline static constexpr int32 max_separation_neighbours{
        simulation::fighters::separation_neighbour_limit};

    /* **************************************** */
    // Navigation
    /* **************************************** */
    auto get_navigation_tick_period(NavigationRiskTier tier) const
        -> FPeriodicTickCountdown16::counter_type;
    auto get_native_navigation_state() -> ml::simulation::fighters::NavigationStateView;
    void reset_navigation_state(int32 fighter_index, NavigationRiskTier initial_tier);

    /* **************************************** */
    // Combat
    /* **************************************** */
    static auto make_fire_point_candidate(FVector3f target_location,
                                          FVector3f reference_location,
                                          float fire_point_distance,
                                          float trace_end_offset,
                                          float desired_attack_distance,
                                          uint32 integral_bias,
                                          float float_bias,
                                          uint32 candidate_order) -> FirePointCandidate;

    /* **************************************** */
    // Simulation phases
    /* **************************************** */
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

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_new_spawn_entity_data() const -> auto const& { return new_spawn_entity_data; }
    auto get_new_spawn_entity_handles() const -> auto const& { return new_spawn_entity_handles; }
    auto get_task_view(Task task) noexcept -> TaskView const&;
    auto get_const_task_view(Task task) const noexcept -> ConstTaskView const&;
    auto find_index(FRegistryEntityHandle fighter_handle) const noexcept -> int32;
    auto get_task_spans() const -> TaskSpans;
    auto get_task_span(Task task) const -> FIndexSpan;
    auto get_task_counts() const -> TaskCounts;

    /* **************************************** */
    // Movement
    /* **************************************** */
    void move(float dt, TaskView const& task_span);
    void update_navigation_steering();
    void collect_navigation_updates(NavigationScratch& scratch);
    void update_separation_observations(NavigationScratch& scratch);
    void apply_separation_steering();
    void scan_preferred_navigation(NavigationScratch& scratch,
                                   float clearance,
                                   float lookahead_time,
                                   float minimum_distance);
    void scan_alternative_navigation(NavigationScratch& scratch,
                                     float clearance,
                                     float lookahead_time,
                                     float minimum_distance);
    void execute_navigation_sweeps(NavigationScratch& scratch, float clearance);
    void select_navigation_alternatives(NavigationScratch& scratch, float safe_progress_time);
    void apply_navigation_choices(NavigationScratch const& scratch);
    void publish_navigation_telemetry() const;

    /* **************************************** */
    // Combat
    /* **************************************** */
    void handle_firing(TaskView const& data);

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto queue_spawns(TestCapitalShipFighterSpawnQueueConstView queue) -> int32;
    void commit_spawns();

    /* **************************************** */
    // Destruction
    /* **************************************** */
    void self_destruct_fighter(FRegistryEntityHandle handle);
    void remove_dead_entities();

    /* **************************************** */
    // Entity data
    /* **************************************** */
    void prepare_entity_update_data();
    bool tasks_are_contiguous() const noexcept;
    void refresh_layout();

    /* **************************************** */
    // Targets
    /* **************************************** */
    void set_target_handle_unchecked(int32 fighter_index,
                                     FRegistryEntityHandle new_target) noexcept;
    void set_target_handle(FRegistryEntityHandle fighter_handle,
                           FRegistryEntityHandle new_target) noexcept;
    void refresh_target_data();

    /* **************************************** */
    // Tasks
    /* **************************************** */
    void set_task_unchecked(int32 index, Task task) noexcept;
    void set_task(FRegistryEntityHandle handle, Task task) noexcept;
    void refresh_task_views();

    /* **************************************** */
    // Orders
    /* **************************************** */
    void queue_orders(TestCapitalShipFighterOrderQueue const& queue);
    void commit_orders();

    /* **************************************** */
    // Misc
    /* **************************************** */
    void clear_tick_buffers();

    friend class CommandInterface;
    friend class PhaseInterface;

    FFighterSimulationConfig config{};
    TStaticTeamArray<uint8> participant_mask{};
    TStaticTeamArray<int32> remaining_team_capacity{};
    int32 per_team_limit{};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    FTickCountdown16::counter_type attack_retry_cooldown_tick_value{0};
    TStaticArray<FPeriodicTickCountdown16::counter_type,
                 static_cast<int32>(NavigationRiskTier::Count)>
        navigation_tick_periods{};
    float minimum_navigation_lookahead_time{};

    EntityBuffers entity_buffers{};
    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager const& spatial_query_manager;
    std::pmr::memory_resource& frame_memory_resource;
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

    ml::test_lasers::Simulation& laser_simulation;

    FNavigationTelemetrySnapshot navigation_telemetry;
    int32 diagnostic_stop_reports{};
    int32 diagnostic_spawn_reports{};
};
} // namespace ml::test_capital_ship_fighters
