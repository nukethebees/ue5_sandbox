#pragma once

#include <SpaceGame/simulation/LevelSimulationConfig.h>

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
#include <SpaceGame/simulation/TraceHits.h>
#include <SpaceGame/support/IndexSpan.h>

#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/tick_countdown.h>

#include <Containers/ArrayView.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

#include <array>

class ATestBatchOrchestrator;
struct FLevelSimulation;
struct FFighterPresentation;
struct FFighterSimulationConfig;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_capital_ship_fighters {
class CommandInterface;
class PhaseInterface;

struct FNavigationTelemetrySnapshot {
    int32 separating_fighter_count{};
    int32 avoiding_fighter_count{};
    int32 clear_risk_count{};
    int32 nearby_risk_count{};
    int32 active_risk_count{};
    int32 immediate_risk_count{};
    int32 separation_query_count{};
    int32 separation_candidate_count{};
    int32 dense_direction_selection_count{};
    int32 steering_memory_fighter_count{};
    int32 hard_trace_count{};
};

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

    Simulation(FSimulationClock const& clock,
               FTestEntityRegistry& entity_registry,
               FSpatialQueryManager const& spatial_query_manager,
               ml::test_lasers::Simulation& laser_simulation) noexcept;
    Simulation(Simulation const&) = delete;
    Simulation(Simulation&&) = delete;
    auto operator=(Simulation const&) -> Simulation& = delete;
    auto operator=(Simulation&&) -> Simulation& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    void set_config(FFighterSimulationConfig const& new_config) noexcept;

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
    struct AvoidanceFrame {
        FVector3f preferred_direction;
        FVector3f first_lateral;
        FVector3f second_lateral;
        float roll_sin;
        float roll_cos;
    };
    struct FirePointAngleOffset {
        float yaw;
        float pitch;
    };
    struct FirePointCandidate {
        FVector3f location;
        FVector3f trace_start;
        FVector3f trace_end;
    };
    enum class NavigationRiskTier : uint8 {
        Clear,
        Nearby,
        Active,
        Immediate,
        Count,
    };

    inline static constexpr int8 direct_movement_choice{-1};
    inline static constexpr int8 stop_movement_choice{-2};
    inline static constexpr int32 n_avoidance_choices{8};
    inline static constexpr uint8 clear_scans_to_end_avoidance{2};
    inline static constexpr uint8 lower_risk_scans_to_demote{2};
    inline static constexpr int32 max_separation_neighbours{32};
    inline static constexpr float crowd_goal_score_weight{0.35f};
    inline static constexpr float steering_memory_score_weight{0.25f};
    inline static constexpr float half_weight{0.5f};
    inline static constexpr float sqrt_three_over_two{0.8660254f};
    inline static constexpr float escape_forward_weight{-0.1736482f};
    inline static constexpr float escape_lateral_weight{0.9848078f};
    inline static constexpr std::array<FirePointAngleOffset, 16> fire_point_angle_offsets{{
        {0.f, 0.f},
        {45.f, 0.f},
        {-45.f, 0.f},
        {90.f, 0.f},
        {-90.f, 0.f},
        {135.f, 0.f},
        {-135.f, 0.f},
        {180.f, 0.f},
        {0.f, 35.f},
        {90.f, 35.f},
        {180.f, 35.f},
        {-90.f, 35.f},
        {45.f, -35.f},
        {135.f, -35.f},
        {-135.f, -35.f},
        {-45.f, -35.f},
    }};

    /* **************************************** */
    // Navigation
    /* **************************************** */
    static auto is_avoidance_direction_choice(int8 choice) -> bool;
    static auto make_avoidance_frame(FVector3f preferred_direction, float float_bias)
        -> AvoidanceFrame;
    static auto make_avoidance_direction(AvoidanceFrame const& frame, int8 choice) -> FVector3f;
    static auto make_avoidance_directions(AvoidanceFrame const& frame)
        -> TStaticArray<FVector3f, n_avoidance_choices>;
    static auto make_avoidance_choice_order(uint32 integral_bias, int8 previous_choice)
        -> TStaticArray<int8, n_avoidance_choices>;
    static auto make_coincident_separation_direction(FRegistryEntityHandle self,
                                                     FRegistryEntityHandle other) -> FVector3f;
    auto get_navigation_tick_period(NavigationRiskTier tier) const
        -> FPeriodicTickCountdown16::counter_type;
    void update_navigation_risk(int32 fighter_index, NavigationRiskTier observed_tier);
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
    void collect_navigation_updates();
    void update_separation_observations();
    void apply_separation_steering();
    void scan_preferred_navigation(float clearance, float lookahead_time, float minimum_distance);
    void scan_alternative_navigation(float clearance, float lookahead_time, float minimum_distance);
    void execute_navigation_sweeps(float clearance);
    void select_navigation_alternatives(float safe_progress_time);
    void apply_navigation_choices();
    void publish_navigation_telemetry() const;

    /* **************************************** */
    // Combat
    /* **************************************** */
    void handle_firing(TaskView const& data);

    /* **************************************** */
    // Spawning
    /* **************************************** */
    void queue_spawns(TestCapitalShipFighterSpawnQueue const& queue);
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
    void clear_presentation_events();

    friend class CommandInterface;
    friend class PhaseInterface;
    friend struct ::FFighterPresentation;

    FFighterSimulationConfig config{};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    FTickCountdown16::counter_type attack_retry_cooldown_tick_value{0};
    TStaticArray<FPeriodicTickCountdown16::counter_type,
                 static_cast<int32>(NavigationRiskTier::Count)>
        navigation_tick_periods{};
    float minimum_navigation_lookahead_time{};

    EntityBuffers entity_buffers{};
    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager const& spatial_query_manager;
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
    ml::test_lasers::SpawnRequests new_lasers;
    TArray<float> aiming_dot_product_buffer;

    TArray<int32> scratch_int_buffer;
    FVectors3f line_of_sight_starts;
    FVectors3f line_of_sight_ends;
    TArray<uint8> line_of_sight_results;
    TArray<FRegistryEntityHandle> firing_ignored_entities;
    TArray<int32> firing_position_fighter_indices;
    FVectors3f firing_position_candidates;
    FTraceHits navigation_trace_hits;
    TArray<int32> navigation_blocked_fighter_indices;
    TArray<int32> navigation_trace_fighter_indices;
    TArray<int8> navigation_trace_choice_indices;
    TArray<uint8> navigation_observed_risk_tiers;

    FNavigationTelemetrySnapshot navigation_telemetry;
    int32 diagnostic_stop_reports{};
    int32 diagnostic_spawn_reports{};

    TArray<int32> presentation_indices_to_remove;
    int32 presentation_spawn_offset{0};
    int32 presentation_spawn_count{0};
};
} // namespace ml::test_capital_ship_fighters
