#pragma once
#include <cstdint>
#include <optional>
#include <sandbox/simulation/fighter_firing_position.h>
#include <sandbox/simulation/fighter_navigation.h>
#include <sandbox/simulation/fighter_navigation_scratch.h>
#include <sandbox/simulation/fighter_navigation_state.h>
#include <sandbox/simulation/fighter_task_layout.h>
#include <sandbox/simulation/navigation_telemetry.h>
#include <span>
#include <vector>

#include <sandbox/simulation/simulation/SystemReadViews.h>

#include <sandbox/simulation/simulation/LevelSimulationConfig.h>

#include <sandbox/simulation/combat/lasers/TestLasersSimulation.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/entity_death_info.h>
#include <sandbox/simulation/entity_handle.h>
#include <sandbox/simulation/fighter_entity_data.h>
#include <sandbox/simulation/fighter_order_queue.h>
#include <sandbox/simulation/fighter_spawn_queue.h>
#include <sandbox/simulation/fighter_types.h>
#include <sandbox/simulation/index_span.h>
#include <sandbox/simulation/registry_entity_data.h>
#include <sandbox/simulation/simulation/SimulationClock.h>
#include <sandbox/simulation/simulation/TraceHits.h>

#include <sandbox/core/multi_buffer.h>

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

struct Simulation {
    using RegistryEntityData = ml::simulation::RegistryEntityData;
    using EntityData = ml::simulation::FighterEntityData;
    using EntityBuffers = ml::MultiBuffer<EntityData, 2>;
    using Task = ml::simulation::CapitalShipFighterTask;
    static constexpr auto n_task_types{ml::simulation::fighters::task_type_count};
    using TaskSpans = ml::simulation::fighters::TaskSpans;
    using TaskCounts = ml::simulation::fighters::TaskCounts;
    using TaskView = EntityData::View;
    using ConstTaskView = EntityData::ConstView;
    using TaskViews = std::array<TaskView, n_task_types>;
    using ConstTaskViews = std::array<ConstTaskView, n_task_types>;

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
                    std::span<ml::simulation::Team const> participating_teams) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const& {
        return entity_registry;
    }
    auto get_laser_simulation() const noexcept -> ml::test_lasers::Simulation const& {
        return laser_simulation;
    }
    auto get_view(std::int32_t offset, std::int32_t width) -> EntityData::View;
    auto get_const_view(std::int32_t offset, std::int32_t width) const -> EntityData::ConstView;
    auto get_handles() const noexcept -> std::span<FRegistryEntityHandle const>;
    auto get_locations() const { return entity_buffers.current().locations.get_view(); }
    auto has_handle(FRegistryEntityHandle fighter_handle) const -> bool;
    auto get_target_handles() const noexcept -> std::span<FRegistryEntityHandle const>;
    auto get_target_handle(FRegistryEntityHandle fighter_handle) const noexcept
        -> FRegistryEntityHandle;
    auto get_target_locations() const {
        return entity_buffers.current().target_locations.get_view();
    }
    auto get_target_location(FRegistryEntityHandle fighter_handle) const
        -> ml::simulation::Vector3f;
    auto get_tasks() const -> std::span<Task const>;
    auto get_teams() const -> std::span<ml::simulation::Team const>;
    auto get_navigation_telemetry() const noexcept -> FNavigationTelemetrySnapshot const& {
        return navigation_telemetry;
    }

    /* **************************************** */
    // Checks
    /* **************************************** */
#ifndef NDEBUG
    void validate_array_sizes() const;
    void check_fighter_tasks() const;
#else
    void validate_array_sizes() const {}
    void check_fighter_tasks() const {}
#endif

    float collision_radius{0.f};
    float fire_point_distance{0.f};
    float fire_dot_product_threshold{0.95f};
    bool diagnostics_enabled{};
  private:
    using FirePointCandidate = ml::simulation::fighters::FirePointCandidate;
    using NavigationScratch = ml::simulation::fighters::NavigationScratch;
    using NavigationRiskTier = ml::simulation::fighters::NavigationRiskTier;

    inline static constexpr std::int8_t direct_movement_choice{-1};
    inline static constexpr std::int8_t stop_movement_choice{-2};
    inline static constexpr std::int32_t n_avoidance_choices{
        ml::simulation::fighters::avoidance_direction_count};
    inline static constexpr std::uint8_t clear_scans_to_end_avoidance{2};
    inline static constexpr std::uint8_t lower_risk_scans_to_demote{2};
    inline static constexpr std::int32_t max_separation_neighbours{
        simulation::fighters::separation_neighbour_limit};

    /* **************************************** */
    // Navigation
    /* **************************************** */
    auto get_navigation_tick_period(NavigationRiskTier tier) const -> std::int16_t;
    auto get_native_navigation_state() -> ml::simulation::fighters::NavigationStateView;
    void reset_navigation_state(std::int32_t fighter_index, NavigationRiskTier initial_tier);

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
    auto find_index(FRegistryEntityHandle fighter_handle) const noexcept -> std::int32_t;
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
    auto queue_spawns(ml::simulation::TestCapitalShipFighterSpawnQueueConstView queue)
        -> std::int32_t;
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
    void set_target_handle_unchecked(std::int32_t fighter_index,
                                     FRegistryEntityHandle new_target) noexcept;
    void set_target_handle(FRegistryEntityHandle fighter_handle,
                           FRegistryEntityHandle new_target) noexcept;
    void refresh_target_data();

    /* **************************************** */
    // Tasks
    /* **************************************** */
    void set_task_unchecked(std::int32_t index, Task task) noexcept;
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
    std::array<std::uint8_t, static_cast<std::size_t>(ml::simulation::Team::COUNT)>
        participant_mask{};
    std::array<std::int32_t, static_cast<std::size_t>(ml::simulation::Team::COUNT)>
        remaining_team_capacity{};
    std::int32_t per_team_limit{};
    FSimulationClock const& simulation_clock;
    std::int16_t attack_retry_cooldown_tick_value{0};
    std::array<std::int16_t, static_cast<std::int32_t>(NavigationRiskTier::Count)>
        navigation_tick_periods{};
    float minimum_navigation_lookahead_time{};

    std::int8_t awareness_restart_ticks_{};
    std::int16_t reposition_restart_ticks_{};
    std::int16_t attack_restart_ticks_{};
    std::int8_t awareness_cleaner_{};
    std::int16_t reposition_cleaner_{};
    std::int16_t attack_cleaner_{};

    EntityBuffers entity_buffers{};
    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager const& spatial_query_manager;
    std::pmr::memory_resource& frame_memory_resource;
    RegistryEntityData registry_update_data;

    ml::simulation::TestCapitalShipFighterSpawnQueue spawn_queue;
    RegistryEntityData new_spawn_entity_data;
    ml::simulation::SpawnedEntityHandles new_spawn_entity_handles;

    std::vector<std::int32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;

    TaskSpans task_spans{};
    TaskViews task_views{};
    ConstTaskViews const_task_views{};
    TestCapitalShipFighterOrderQueue order_queue{};

    ml::test_lasers::Simulation& laser_simulation;

    FNavigationTelemetrySnapshot navigation_telemetry;
    std::int32_t diagnostic_stop_reports{};
    std::int32_t diagnostic_spawn_reports{};
};
} // namespace ml::test_capital_ship_fighters
