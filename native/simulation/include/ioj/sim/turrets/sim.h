#pragma once
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_frame_change.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_mission_event_groups.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/turrets/phase_inputs.h>
#include <ioj/sim/turrets/turret_entity_data.h>
#include <ioj/sim/turrets/turret_sim_config.h>

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSim;
class EntityLedger;
class LevelSpawnManager;
}

namespace ioj::sim::turrets {
class PhaseInterface;

struct Sim {
    using EntityStorage = TurretEntityData;

    Sim(SimClock const& clock, EntityLedger& ledger, EntityTables& entity_tables) noexcept;
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
    auto get_target_ids() const -> std::span<EntityId const>;

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
    void think(ThinkingInputs const inputs);
    void generate_fire_commands(ThinkingInputs const inputs);
    void resolve_damage_events(ResolutionInputs const inputs);
    void publish_deaths();
    void remove_components();
    void remove_entities();
    void finish_action();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto register_turrets(LevelTurretSpawnEvents::ConstView const spawn_data)
        -> std::vector<EntityId>;

    /* **************************************** */
    // Entity data
    /* **************************************** */

    /* **************************************** */
    // Searching
    /* **************************************** */
    void perform_search(ThinkingInputs const inputs);
    void refresh_target_data(ThinkingInputs const inputs);
    void perform_search_on_slice(std::uint32_t begin,
                                 std::uint32_t end,
                                 float radius,
                                 ThinkingInputs const inputs);

    /* **************************************** */
    // Attacking
    /* **************************************** */
    void fire_at_enemies(ThinkingInputs const inputs);
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
    EntityTables& entity_tables_;
    EntityStorage entities{};
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;
    std::vector<Vector3f> death_locations_;
    std::int32_t target_refresh_next_offset{0};
    NonNegative<std::int16_t> cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};

    std::vector<std::uint32_t> local_indices_to_remove;
};
} // namespace ioj::sim::turrets
