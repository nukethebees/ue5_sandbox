#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/fighter_navigation.h>
#include <ioj/sim/fighter_navigation_scratch.h>
#include <ioj/sim/navigation_telemetry.h>
#include <span>
#include <vector>

#include <ioj/sim/system_read_views.h>

#include <ioj/sim/sim_config.h>

#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/fighter_order_queue.h>
#include <ioj/sim/fighter_spawn_queue.h>
#include <ioj/sim/fighter_types.h>
#include <ioj/sim/health_table.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/trace_hits.h>

#include <sandbox/core/frame_memory_resource.h>
#include <sandbox/core/multi_buffer.h>

#include <array>

namespace ioj::sim {
class EntityLedger;
class CombatEvents;
struct SpatialQueryManager;
}

namespace ioj::sim::fighters {
class CommandInterface;
class PhaseInterface;

struct FighterLevelData {
    std::span<Team const> participating_teams;
    float collision_radius{};
    float fire_point_distance{};
};

struct Sim {
    using EntityData = FighterEntityData;
    using EntityStorage = SingleAllocationFighterEntityData;
    using EntityBuffers = ml::MultiBuffer<EntityStorage, 2>;
    using Task = FighterTask;
    static constexpr auto n_task_types{static_cast<std::size_t>(Task::COUNT)};
    using TaskSpans = std::array<IndexSpan, n_task_types>;
    using TaskCounts = std::array<std::int32_t, n_task_types>;
    using TaskView = EntityData::View;
    using ConstTaskView = EntityData::ConstView;

    Sim(SimClock const& clock,
        EntityLedger& ledger,
        CombatEvents const& combat_events,
        HealthTable& health_table,
        AgentAccessor const& agents,
        SpatialQueryManager const& spatial_query_manager,
        lasers::Sim& laser_simulation) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> FighterReadView {
        auto const entities{entity_buffers.current().get_const_view().columns()};
        return {entities, health_table_.get_const_view(entities.health_indices)};
    }
    void set_config(FighterSimConfig const& new_config, FighterLevelData level_data) noexcept;
    void set_diagnostics_enabled(bool enabled) noexcept { diagnostics_enabled_ = enabled; }

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_collision_dirty_entities() const noexcept -> std::span<EntityUniqueId const> {
        return collision_dirty_entities_;
    }
    auto get_laser_simulation() const noexcept -> lasers::Sim const& { return laser_simulation; }
    auto get_view(std::int32_t offset, std::int32_t width) -> EntityData::View;
    auto get_const_view(std::int32_t offset, std::int32_t width) const -> EntityData::ConstView;
    auto get_entity_ids() const -> std::span<EntityUniqueId const> {
        return entity_buffers.current().get_const_view().entity_ids();
    }
    auto get_parent_ids() const -> std::span<EntityUniqueId const> {
        return entity_buffers.current().get_const_view().parent_ids();
    }
    auto get_healths() const -> HealthConstView {
        auto const entities{entity_buffers.current().get_const_view().columns()};
        return health_table_.get_const_view(entities.health_indices);
    }
    void set_parent_id(EntityUniqueId fighter, EntityUniqueId parent) {
        assert(fighter.is_valid() && fighter.entity_type() == EntityType::Fighter);
        auto const index{agents_.indexes().find(fighter)};
        assert(index >= 0);
        entity_buffers.current().get_view().parent_ids()[index] = parent;
    }
    auto get_locations() const {
        return entity_buffers.current().get_const_view().columns().locations;
    }
    auto has_id(EntityUniqueId fighter) const -> bool;
    auto get_target_ids() const noexcept -> std::span<EntityUniqueId const>;
    auto get_target_id(EntityUniqueId fighter) const noexcept -> EntityUniqueId;
    auto get_target_locations() const {
        return entity_buffers.current().get_const_view().columns().target_locations;
    }
    auto get_target_location(EntityUniqueId fighter) const -> Vector3f;
    auto get_tasks() const -> std::span<Task const>;
    auto get_teams() const -> std::span<Team const>;
    auto get_navigation_telemetry() const noexcept -> NavigationTelemetrySnapshot const& {
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
  private:
    using NavigationScratch = fighters::NavigationScratch;
    using NavigationRiskTier = fighters::NavigationRiskTier;

    inline static constexpr std::int8_t direct_movement_choice{-1};
    inline static constexpr std::int8_t stop_movement_choice{-2};
    inline static constexpr std::int32_t n_avoidance_choices{avoidance_direction_count};
    inline static constexpr std::uint8_t clear_scans_to_end_avoidance{2};
    inline static constexpr std::uint8_t lower_risk_scans_to_demote{2};
    inline static constexpr std::int32_t max_separation_neighbours{separation_neighbour_limit};

    /* **************************************** */
    // Navigation
    /* **************************************** */
    auto get_navigation_tick_period(NavigationRiskTier tier) const -> std::int16_t;
    void reset_navigation_state(std::int32_t fighter_index, NavigationRiskTier initial_tier);

    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void prepare_tick(float dt);
    void think(float dt, ml::FrameScratch& scratch);
    void plan_movement(float dt, ml::FrameScratch& scratch);
    void apply_movement(ml::FrameScratch& scratch);
    void generate_fire_commands(ml::FrameScratch& scratch);
    void resolve_damage_events();
    void publish_deaths();
    void cleanup_entities();
    void finish_action();

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_task_view(Task task) noexcept -> TaskView;
    auto get_const_task_view(Task task) const noexcept -> ConstTaskView;
    auto find_index(EntityUniqueId fighter) const noexcept -> std::int32_t;
    auto get_task_spans() const -> TaskSpans;
    auto get_task_span(Task task) const -> IndexSpan;
    auto get_task_counts() const -> TaskCounts;

    /* **************************************** */
    // Movement
    /* **************************************** */
    void move(float dt, TaskView const& task_span, ml::FrameScratch& scratch);
    void update_navigation_steering(ml::FrameScratch& scratch);
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
    void handle_firing(TaskView const& data, ml::FrameScratch& scratch);

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto queue_spawns(FighterSpawnQueueConstView queue) -> std::int32_t;
    void reassign_pending_spawns(EntityUniqueId parent, EntityUniqueId replacement);
    void commit_spawns();

    /* **************************************** */
    // Destruction
    /* **************************************** */
    void remove_dead_entities();

    /* **************************************** */
    // Entity data
    /* **************************************** */
    bool tasks_are_contiguous() const noexcept;
    void refresh_layout();

    /* **************************************** */
    // Targets
    /* **************************************** */
    void set_target_id_unchecked(std::int32_t fighter_index, EntityUniqueId new_target) noexcept;
    void set_target_id(EntityUniqueId fighter, EntityUniqueId new_target) noexcept;
    void refresh_target_data(ml::FrameScratch& scratch);

    /* **************************************** */
    // Tasks
    /* **************************************** */
    void set_task_unchecked(std::int32_t index, Task task) noexcept;
    void set_task(EntityUniqueId fighter, Task task) noexcept;

    /* **************************************** */
    // Orders
    /* **************************************** */
    void queue_orders(FighterOrderQueue const& queue);
    void commit_orders();

    /* **************************************** */
    // Misc
    /* **************************************** */
    void clear_tick_buffers();

    friend class CommandInterface;
    friend class PhaseInterface;

    float movement_tick_period_{};
    FighterSimConfig config{};
    float collision_radius_{};
    float fire_point_distance_{};
    bool diagnostics_enabled_{};
    std::array<std::uint8_t, static_cast<std::size_t>(Team::COUNT)> participant_mask{};
    std::array<std::int32_t, static_cast<std::size_t>(Team::COUNT)> remaining_team_capacity{};
    std::int32_t per_team_limit{};
    SimClock const& simulation_clock;
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
    EntityLedger& ledger_;
    CombatEvents const& combat_events_;
    HealthTable& health_table_;
    AgentAccessor const& agents_;
    SpatialQueryManager const& spatial_query_manager;

    SingleAllocationFighterSpawnQueue spawn_queue;

    std::vector<std::int32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;

    TaskSpans task_spans{};
    FighterOrderQueue order_queue{};

    lasers::Sim& laser_simulation;
    std::vector<std::int32_t> pending_fire_indices_;
    std::vector<EntityUniqueId> collision_dirty_entities_;

    NavigationTelemetrySnapshot navigation_telemetry;
    std::int32_t diagnostic_stop_reports{};
    std::int32_t diagnostic_spawn_reports{};
};
} // namespace ioj::sim::fighters
