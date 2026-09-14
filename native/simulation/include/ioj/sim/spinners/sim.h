#pragma once
#include <cstdint>
#include <ioj/sim/system_read_views.h>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/sim_config.h>

#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/spinner_entity_data.h>

#include <memory_resource>
#include <vector>

namespace ioj::sim {
struct LevelSim;
struct SpinnerSimConfig;
struct EntityRegistry;
}

namespace ioj::sim::spinners {
class PhaseInterface;

struct Sim {
    using EntityData = ioj::sim::SpinnerEntityData;

    Sim(SimClock const& clock,
        EntityRegistry& entity_registry,
        ioj::sim::lasers::Sim& laser_simulation,
        std::pmr::memory_resource& frame_memory_resource) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> SpinnerReadView { return {entities.get_const_view()}; }
    void set_config(SpinnerSimConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_entity_registry() const -> EntityRegistry const& { return entity_registry; }
    auto get_laser_simulation() const -> ioj::sim::lasers::Sim const& { return laser_simulation; }

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void update_timers(float dt);
    void move(float dt);
    void queue_commands();
    void end_tick();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    void spawn_instances(ioj::sim::Vectors3fConstView new_locations,
                         std::span<float const> new_yaws,
                         std::span<std::int32_t const> new_fire_point_indices);

    /* **************************************** */
    // Movement
    /* **************************************** */
    void rotate_instances(float dt);

    /* **************************************** */
    // Firing
    /* **************************************** */
    void fire_lasers();

    friend class PhaseInterface;
    friend struct ::ioj::sim::LevelSim;

    friend struct SpinnerSpawnTestAccess;

    SpinnerSimConfig config{};
    std::int16_t cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};
    SimClock const& simulation_clock;
    EntityRegistry& entity_registry;
    ioj::sim::lasers::Sim& laser_simulation;
    std::pmr::memory_resource& frame_memory_resource;
    EntityData entities{};
};
} // namespace ioj::sim::spinners
