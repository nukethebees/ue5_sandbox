#pragma once
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/collision/trace_hits.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/fighters/fighter_entity_data.h>
#include <ioj/sim/fighters/fighter_navigation.h>
#include <ioj/sim/fighters/fighter_navigation_scratch.h>
#include <ioj/sim/fighters/fighter_sim_config.h>
#include <ioj/sim/fighters/fighter_spawn_queue.h>
#include <ioj/sim/fighters/phase_inputs.h>
#include <ioj/sim/telemetry/navigation_telemetry.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSim;
}

namespace ioj::sim::fighters {
class PhaseInterface;

struct FighterLevelData {
    std::span<Team const> participating_teams;
    SimClock const& clock;
};

struct Sim {
    using EntityStorage = FighterEntityData;

    explicit Sim(EntityTables& entity_tables) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto entities() const -> EntityStorage::ConstView { return entities_.const_view(); }
    auto num_instances() const noexcept -> std::uint32_t;
    auto healths() const -> HealthConstView {
        return entity_tables_.health.const_view<EntityType::Fighter>();
    }
    auto get_navigation_telemetry() const noexcept -> telemetry::FighterNavigationSnapshot const& {
        return navigation_telemetry;
    }

    /* **************************************** */
    // Spawning
    /* **************************************** */
    // Queue a single-team wave for the next Preparation, subject to the team's live cap.
    auto queue_spawns(FighterSpawnQueue::ConstView new_spawns) -> std::uint32_t;

  private:
    using NavigationScratch = fighters::NavigationScratch;
    using NavigationRiskTier = fighters::NavigationRiskTier;

    static constexpr std::uint32_t n_avoidance_choices{avoidance_direction_count};
    static constexpr std::uint8_t clear_scans_to_end_avoidance{2};
    static constexpr std::uint8_t lower_risk_scans_to_demote{2};
    static constexpr std::uint32_t max_separation_neighbours{separation_neighbour_limit};

    /* **************************************** */
    // Navigation
    /* **************************************** */
    auto navigation_tick_period(NavigationRiskTier tier) const -> std::int16_t;

    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void initialise(FighterSimConfig const& new_config, FighterLevelData level_data);
    void update_entity_lookup_table();
    void prepare_tick();
    void think(ThinkingInputs const inputs);
    void plan_movement(ThinkingInputs const inputs);
    void apply_movement(ActionInputs const inputs);
    void generate_fire_commands(ThinkingInputs const inputs);
    void resolve_damage_events(ResolutionInputs const inputs);
    void publish_deaths(ResolutionInputs inputs);
    void remove_components();
    void remove_entities();
    void finish_action();

    /* **************************************** */
    // Movement
    /* **************************************** */
    void move(EntityStorage::View fighters, ActionInputs inputs);
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
    void handle_firing(EntityStorage::View data, ThinkingInputs const inputs);

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto accept_spawn_count(std::span<Team const> teams) -> std::uint32_t;
    void commit_spawns(PreparationInputs inputs);

    /* **************************************** */
    // Destruction
    /* **************************************** */
    void remove_dead_entities();

    /* **************************************** */
    // Targets
    /* **************************************** */
    void refresh_target_data(ThinkingInputs const inputs);

    /* **************************************** */
    // Misc
    /* **************************************** */
    void clear_tick_buffers();

    friend class PhaseInterface;

    FighterSimConfig config{};
    std::array<std::uint8_t, enum_count<Team>()> participant_mask{};
    std::array<std::int32_t, enum_count<Team>()> remaining_team_capacity{};
    std::int32_t per_team_limit{};
    NonNegative<std::int16_t> attack_retry_cooldown_tick_value{};
    std::array<std::int16_t, enum_count<NavigationRiskTier>()> navigation_tick_periods{};
    float minimum_navigation_lookahead_time{};

    NonNegative<std::int16_t> attack_restart_ticks_{};
    std::int16_t attack_cleaner_{};

    EntityStorage entities_{};
    EntityTables& entity_tables_;

    FighterSpawnQueue spawn_queue;

    std::vector<std::uint32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;

    telemetry::FighterNavigationSnapshot navigation_telemetry;
};
} // namespace ioj::sim::fighters
