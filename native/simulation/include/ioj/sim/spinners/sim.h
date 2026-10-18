#pragma once
#include <ioj/column_math.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/spinners/spinner_entity_data.h>
#include <ioj/sim/spinners/spinner_sim_config.h>
#include <ioj/vectors3f.h>

#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSim;
class LevelSpawnManager;
class EntityLedger;
}

namespace ioj::sim::spinners {
class PhaseInterface;

struct Sim {
    using EntityStorage = SpinnerEntityData;

    Sim(SimClock const& clock,
        EntityLedger& ledger,
        EntityTables& entity_tables,
        lasers::Sim& laser_simulation) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_entities() const -> EntityStorage::ConstView { return entities.get_const_view(); }
    void set_config(SpinnerSimConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::uint32_t;
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
    void think(float dt);
    void apply_movement(FrameMemoryResource* const scratch_resource);
    void generate_fire_commands();
    void materialize_fire_commands();
    std::vector<std::uint32_t> pending_fire_indices_;
    void finish_action();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    auto spawn_instances(Vectors3fConstView const new_locations,
                         std::span<float const> const new_yaws,
                         std::span<std::int32_t const> const new_fire_point_indices)
        -> std::span<EntityId const>;

    /* **************************************** */
    // Movement
    /* **************************************** */

    /* **************************************** */
    // Firing
    /* **************************************** */
    void fire_lasers();

    friend class PhaseInterface;
    friend struct sim::LevelSim;
    friend class sim::LevelSpawnManager;

    friend struct SpinnerSpawnTestAccess;

    float planned_yaw_delta_{};
    SpinnerSimConfig config{};
    std::int16_t cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};
    SimClock const& simulation_clock;
    EntityLedger& ledger_;
    EntityTables& entity_tables_;
    lasers::Sim& laser_simulation;
    EntityStorage entities{};
};
} // namespace ioj::sim::spinners
