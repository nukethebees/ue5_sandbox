#pragma once
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_frame_change.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/levels/level_runtime_events.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/sim_config.h>
#include <ioj/sim/turret_entity_data.h>

#include <sandbox/core/frame_memory_resource.h>

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

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
    using EntityStorage = TurretEntityData;

    Sim(SimClock const& clock,
        EntityLedger& ledger,
        CombatEvents const& combat_events,
        EntityTables& entity_tables,
        SpatialQueryManager const& spatial_query_manager,
        lasers::Sim& laser_simulation) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_entities() const -> EntityStorage::ConstView { return entities.get_const_view(); }
    auto get_healths() const -> HealthConstView {
        return entity_tables_.health.get_const_view<EntityType::Turret>(entities.num());
    }
    auto get_frame_changes() const -> std::span<EntityFrameChange const> { return frame_changes_; }
    auto get_death_locations() const -> std::span<Vector3f const> { return death_locations_; }
    void reset_frame_output() {
        frame_changes_.clear();
        death_locations_.clear();
    }
    void set_config(TurretSimConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::uint32_t;
    auto get_target_ids() const -> std::span<EntityUniqueId const>;
    auto get_laser_simulation() const -> lasers::Sim const& { return laser_simulation; }

    /* **************************************** */
    // Checks
    /* **************************************** */
  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void update_entity_lookup_table();
    void prepare_tick(float dt);
    void think(float dt, ml::FrameMemoryResource* const scratch_resource);
    void generate_fire_commands(ml::FrameMemoryResource* const scratch_resource);
    void resolve_damage_events();
    void publish_deaths();
    void remove_components();
    void remove_entities();
    void finish_action();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto register_turrets(LevelTurretSpawnEvents::ConstView const spawn_data)
        -> std::vector<EntityUniqueId>;

    /* **************************************** */
    // Entity data
    /* **************************************** */

    /* **************************************** */
    // Searching
    /* **************************************** */
    void perform_search(ml::FrameMemoryResource* scratch_resource);
    void refresh_target_data(ml::FrameMemoryResource* const scratch_resource);
    void perform_search_on_slice(std::uint32_t begin,
                                 std::uint32_t end,
                                 float radius,
                                 ml::FrameMemoryResource* scratch_resource);

    /* **************************************** */
    // Attacking
    /* **************************************** */
    void fire_at_enemies(ml::FrameMemoryResource* const scratch_resource);
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
    SpatialQueryManager const& spatial_query_manager;
    lasers::Sim& laser_simulation;
    EntityStorage entities{};
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;
    std::vector<Vector3f> death_locations_;
    std::int32_t target_refresh_next_offset{0};
    std::int16_t cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};

    std::vector<std::uint32_t> local_indices_to_remove;
};
} // namespace ioj::sim::turrets
