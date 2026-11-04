#pragma once
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/collision/trace_hits.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/fighters/fighter_entity_data.h>
#include <ioj/sim/fighters/fighter_navigation.h>
#include <ioj/sim/fighters/fighter_navigation_scratch.h>
#include <ioj/sim/fighters/fighter_spawn_queue.h>
#include <ioj/sim/fighters/phase_inputs.h>
#include <ioj/sim/fighters/runtime_config.h>
#include <ioj/team_array.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim {
struct LevelSim;
}

namespace ioj::sim::fighters {
class PhaseInterface;

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

    /* **************************************** */
    // Spawning
    /* **************************************** */
    // Queue a single-team wave for the next Preparation, subject to the team's live cap.
    auto queue_spawns(FighterSpawnQueue::ConstView new_spawns,
                      std::span<Team const> participating_teams) -> std::uint32_t;

  private:
    using NavigationScratch = fighters::NavigationScratch;
    using NavigationRiskTier = fighters::NavigationRiskTier;

    static constexpr std::uint32_t n_avoidance_choices_{avoidance_direction_count};
    static constexpr std::uint8_t clear_scans_to_end_avoidance_{2};
    static constexpr std::uint8_t lower_risk_scans_to_demote_{2};
    static constexpr std::uint32_t max_separation_neighbours_{separation_neighbour_limit};

    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void initialise(FighterSimConfig const& new_config, SimClock const& clock);
    void update_entity_lookup_table();
    void prepare_tick(PreparationInputs inputs);
    void think(ThinkingInputs const inputs);
    void plan_instructions(tasks::TaskManager const& manager);
    void change_task(std::uint32_t row, tasks::TaskId task, tasks::TaskManager const& manager);
    void plan_movement(ThinkingInputs const inputs);
    void apply_movement(ActionInputs const inputs);
    void generate_fire_commands(ThinkingInputs const inputs);
    void resolve_damage_events(ResolutionInputs const inputs);
    void publish_deaths(ResolutionInputs inputs);
    void remove_components(ResolutionCommitInputs inputs);
    void remove_entities(ResolutionCommitInputs inputs);
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

    /* **************************************** */
    // Combat
    /* **************************************** */
    void handle_firing(EntityStorage::View data, ThinkingInputs const inputs);

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto accept_spawn_count(std::span<Team const> teams, std::span<Team const> participating_teams)
        -> std::uint32_t;
    void commit_spawns(PreparationInputs inputs);

    /* **************************************** */
    // Destruction
    /* **************************************** */
    void remove_dead_entities(std::span<std::uint32_t const> local_indices_to_remove);

    /* **************************************** */
    // Targets
    /* **************************************** */
    void refresh_target_data(ThinkingInputs const inputs);

    friend class PhaseInterface;

    RuntimeConfig config_{};
    TeamArray<std::int32_t> remaining_team_capacity_{};
    std::int16_t attack_cleaner_{};
    EntityStorage entities_{};
    EntityTables& entity_tables_;
    FighterSpawnQueue spawn_queue_;
};
} // namespace ioj::sim::fighters
