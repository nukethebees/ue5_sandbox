#pragma once
#include <cstdint>
#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/system_read_views.h>
#include <ioj/sim/turret_spawn_data.h>
#include <sandbox/core/frame_memory_resource.h>
#include <span>
#include <vector>

#include <ioj/sim/sim_config.h>

#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/turret_entity_data.h>

#include <memory_resource>

namespace ioj::sim {
struct LevelSim;
class EntityLedger;
class CombatEvents;
class LevelSpawnManager;
struct SpatialQueryManager;
}

namespace ioj::sim::turrets {
class PhaseInterface;

struct Sim {
    using EntityData = TurretEntityData;
    using EntityStorage = SingleAllocationTurretEntityData;
    using SpawnData = TurretSpawnData;

    Sim(SimClock const& clock,
        EntityLedger& ledger,
        CombatEvents const& combat_events,
        EntityTables& entity_tables,
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
    auto get_read_view() const -> TurretReadView {
        auto const entity_data{entities.get_const_view().columns()};
        return {entity_data,
                entity_tables_.health.get_const_view(entity_data.health_indices,
                                                     entity_data.entity_ids),
                frame_changes_,
                death_locations_};
    }
    void reset_frame_output() {
        frame_changes_.clear();
        death_locations_.clear();
    }
    void set_config(TurretSimConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_target_ids() const -> std::span<EntityUniqueId const>;
    auto get_laser_simulation() const -> lasers::Sim const& { return laser_simulation; }

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void prepare_tick(float dt);
    void think(float dt, ml::FrameScratch& scratch);
    void generate_fire_commands(ml::FrameScratch& scratch);
    void resolve_damage_events();
    void publish_deaths();
    void remove_components();
    void remove_entities();
    void finish_action();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto register_turrets(TurretSpawnDataConstView spawn_data, Rotators3fConstView rotations)
        -> std::vector<EntityUniqueId>;

    /* **************************************** */
    // Entity data
    /* **************************************** */

    /* **************************************** */
    // Searching
    /* **************************************** */
    void perform_search(ml::FrameScratch& scratch);
    void refresh_target_data(ml::FrameScratch& scratch);
    void perform_search_on_slice(std::int32_t job_index,
                                 std::int32_t n_turrets,
                                 std::int32_t turrets_per_job,
                                 float radius);

    /* **************************************** */
    // Attacking
    /* **************************************** */
    void fire_at_enemies(ml::FrameScratch& scratch);
    auto get_disengage_radius() const -> float;

    /* **************************************** */
    // Death handling
    /* **************************************** */
    void handle_dead_entities();

    /* **************************************** */
    // Misc
    /* **************************************** */
    void clear_tick_buffers();

    friend class PhaseInterface;
    friend struct sim::LevelSim;

    friend class sim::LevelSpawnManager;

    TurretSimConfig config{};
    SimClock const& simulation_clock;
    EntityLedger& ledger_;
    CombatEvents const& combat_events_;
    EntityTables& entity_tables_;
    AgentAccessor const& agents_;
    SpatialQueryManager const& spatial_query_manager;
    lasers::Sim& laser_simulation;
    EntityStorage entities{};
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;
    std::vector<Vector3f> death_locations_;
    std::int32_t target_refresh_next_offset{0};
    std::int16_t cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};

    std::vector<std::int32_t> local_indices_to_remove;
};
} // namespace ioj::sim::turrets
