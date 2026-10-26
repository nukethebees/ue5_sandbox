#pragma once
#include <ioj/frame_memory_resource.h>
#include <ioj/multi_buffer.h>
#include <ioj/sim/collision/trace_hits.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/fighters/fighter_entity_data.h>
#include <ioj/sim/fighters/fighter_navigation.h>
#include <ioj/sim/fighters/fighter_navigation_scratch.h>
#include <ioj/sim/fighters/fighter_order_queue.h>
#include <ioj/sim/fighters/fighter_sim_config.h>
#include <ioj/sim/fighters/fighter_spawn_queue.h>
#include <ioj/sim/fighters/fighter_task_format.h>
#include <ioj/sim/fighters/phase_inputs.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/telemetry/navigation_telemetry.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
class EntityLedger;
struct LevelSim;
struct LevelSimTestAccess;
}

namespace ioj::sim::fighters {
class CommandInterface;
class PhaseInterface;

struct FighterLevelData {
    std::span<Team const> participating_teams;
    PositiveFinite<float> collision_radius;
};

struct Sim {
    using EntityStorage = FighterEntityData;
    using EntityBuffers = MultiBuffer<EntityStorage, 2>;
    using Task = FighterTask;
    static constexpr auto n_task_types{enum_count<Task>()};
    using TaskSpans = std::array<IndexSpan, n_task_types>;
    using TaskCounts = std::array<std::uint32_t, n_task_types>;
    using TaskView = EntityStorage::View;
    using ConstTaskView = EntityStorage::ConstView;

    Sim(SimClock const& clock, EntityLedger& ledger, EntityTables& entity_tables) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_entities() const -> EntityStorage::ConstView {
        return entity_buffers.current().get_const_view();
    }
    void set_config(FighterSimConfig const& new_config, FighterLevelData level_data) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::uint32_t;
    auto get_overlap_candidates() const noexcept -> std::span<EntityId const> {
        return overlap_candidates_;
    }
    auto get_const_view(std::uint32_t offset, std::uint32_t width) const
        -> EntityStorage::ConstView;
    auto get_membership_revision() const noexcept -> std::uint64_t { return membership_revision_; }
    auto get_layout_revision() const noexcept -> std::uint64_t { return layout_revision_; }
    auto get_entity_ids() const -> std::span<EntityId const> {
        return entity_buffers.current().get_const_view().entity_ids();
    }
    auto get_parent_ids() const -> std::span<EntityId const> {
        return entity_buffers.current().get_const_view().parent_ids();
    }
    auto get_healths() const -> HealthConstView {
        return entity_tables_.health.get_const_view<EntityType::Fighter>(
            entity_buffers.current().num());
    }
    void set_parent_id(EntityId fighter, EntityId parent);
    auto get_locations() const {
        return entity_buffers.current().get_const_view().view_locations();
    }
    auto has_id(EntityId fighter) const -> bool;
    auto get_target_ids() const noexcept -> std::span<EntityId const>;
    auto get_target_id(EntityId fighter) const noexcept -> EntityId;
    auto get_target_locations() const {
        return entity_buffers.current().get_const_view().view_target_locations();
    }
    auto get_target_location(EntityId fighter) const -> Vector3f;
    auto get_tasks() const -> std::span<Task const>;
    auto get_teams() const -> std::span<Team const>;
    auto get_navigation_telemetry() const noexcept -> NavigationTelemetrySnapshot const& {
        return navigation_telemetry;
    }

    /* **************************************** */
    // Checks
    /* **************************************** */
#ifndef NDEBUG
    void check_fighter_tasks() const;
#else
    void check_fighter_tasks() const {}
#endif
  private:
    using NavigationScratch = fighters::NavigationScratch;
    using NavigationRiskTier = fighters::NavigationRiskTier;

    inline static constexpr std::uint32_t n_avoidance_choices{avoidance_direction_count};
    inline static constexpr std::uint8_t clear_scans_to_end_avoidance{2};
    inline static constexpr std::uint8_t lower_risk_scans_to_demote{2};
    inline static constexpr std::uint32_t max_separation_neighbours{separation_neighbour_limit};

    /* **************************************** */
    // Navigation
    /* **************************************** */
    auto get_navigation_tick_period(NavigationRiskTier tier) const -> std::int16_t;

    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void update_entity_lookup_table();
    void prepare_tick(float dt);
    void think(ThinkingInputs const inputs);
    void plan_movement(ThinkingInputs const inputs);
    void apply_movement(ActionInputs const inputs);
    void generate_fire_commands(ThinkingInputs const inputs);
    void resolve_damage_events(ResolutionInputs const inputs);
    void publish_deaths();
    void remove_components();
    void remove_entities();
    void finish_action();

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_view(std::uint32_t offset, std::uint32_t width) -> EntityStorage::View;
    auto get_task_view(Task task) noexcept -> TaskView;
    auto get_const_task_view(Task task) const noexcept -> ConstTaskView;
    auto find_index(EntityId fighter) const noexcept -> std::uint32_t;
    auto get_task_spans() const -> TaskSpans;
    auto get_task_span(Task task) const -> IndexSpan;
    auto get_task_counts() const -> TaskCounts;

    /* **************************************** */
    // Movement
    /* **************************************** */
    void move(float dt, TaskView fighters, FrameMemoryResource* const scratch_resource);
    void update_navigation_steering(ThinkingInputs const inputs);
    void collect_navigation_updates(NavigationScratch& scratch);
    void update_separation_observations(NavigationScratch& scratch, ThinkingInputs const inputs);
    void apply_separation_steering();
    void scan_preferred_navigation(NavigationScratch& scratch,
                                   float clearance,
                                   float avoidance_lookahead_time,
                                   float minimum_lookahead_distance,
                                   ThinkingInputs const inputs);
    void scan_alternative_navigation(NavigationScratch& scratch,
                                     float clearance,
                                     float avoidance_lookahead_time,
                                     float minimum_lookahead_distance,
                                     ThinkingInputs const inputs);
    void execute_navigation_sweeps(NavigationScratch& scratch,
                                   float clearance,
                                   ThinkingInputs const inputs);
    void select_navigation_alternatives(NavigationScratch& scratch, float safe_progress_time);
    void apply_navigation_choices(NavigationScratch const& scratch);
    void publish_navigation_telemetry() const;

    /* **************************************** */
    // Combat
    /* **************************************** */
    void handle_firing(TaskView data, ThinkingInputs const inputs);

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto queue_spawns(FighterSpawnQueue::ConstView new_spawns) -> std::uint32_t;
    auto accept_spawn_count(std::span<Team const> teams) -> std::uint32_t;
    void reassign_pending_spawns(EntityId parent, EntityId replacement);
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
    void set_target_id_unchecked(std::uint32_t fighter_index, EntityId new_target) noexcept;
    void set_target_id(EntityId fighter, EntityId new_target) noexcept;
    void refresh_target_data(ThinkingInputs const inputs);

    /* **************************************** */
    // Tasks
    /* **************************************** */
    void set_task(EntityId fighter, Task task) noexcept;

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
    friend struct sim::LevelSim;
    friend struct sim::LevelSimTestAccess;

    float movement_tick_period_{};
    FighterSimConfig config{};
    float collision_radius_{};
    std::array<std::uint8_t, enum_count<Team>()> participant_mask{};
    std::array<std::int32_t, enum_count<Team>()> remaining_team_capacity{};
    std::int32_t per_team_limit{};
    SimClock const& simulation_clock;
    NonNegative<std::int16_t> attack_retry_cooldown_tick_value{};
    std::array<std::int16_t, enum_count<NavigationRiskTier>()> navigation_tick_periods{};
    float minimum_navigation_lookahead_time{};

    NonNegative<std::int8_t> awareness_restart_ticks_{};
    NonNegative<std::int16_t> reposition_restart_ticks_{};
    NonNegative<std::int16_t> attack_restart_ticks_{};
    std::int8_t awareness_cleaner_{};
    std::int16_t reposition_cleaner_{};
    std::int16_t attack_cleaner_{};

    EntityBuffers entity_buffers{};
    std::uint64_t membership_revision_{};
    // Membership lists preserve fighter storage order, independently of live ownership changes.
    std::uint64_t layout_revision_{};
    EntityLedger& ledger_;
    EntityTables& entity_tables_;
    std::vector<std::int32_t> layout_order_;

    FighterSpawnQueue spawn_queue;

    std::vector<std::uint32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;

    TaskSpans task_spans{};
    FighterOrderQueue order_queue{};

    std::vector<std::uint32_t> pending_fire_indices_;
    std::vector<EntityId> overlap_candidates_;

    NavigationTelemetrySnapshot navigation_telemetry;
};
} // namespace ioj::sim::fighters
