#pragma once
#include <cstdint>
#include <optional>
#include <sandbox/simulation/simulation/SystemReadViews.h>
#include <sandbox/simulation/spinner_firing.h>
#include <span>
#include <vector>

#include <sandbox/simulation/simulation/LevelSimulationConfig.h>

#include <sandbox/simulation/combat/lasers/TestLasersSimulation.h>
#include <sandbox/simulation/simulation/SimulationClock.h>
#include <sandbox/simulation/spinner_entity_data.h>

#include <memory_resource>
#include <vector>

struct FLevelSimulation;
struct FSpinnerSimulationConfig;
struct FTestEntityRegistry;

namespace ml::test_tube_spinners {
class PhaseInterface;

struct Simulation {
    using EntityData = ml::simulation::SpinnerEntityData;

    Simulation(FSimulationClock const& clock,
               FTestEntityRegistry& entity_registry,
               ml::test_lasers::Simulation& laser_simulation,
               std::pmr::memory_resource& frame_memory_resource) noexcept;
    Simulation(Simulation const&) = delete;
    Simulation(Simulation&&) = delete;
    auto operator=(Simulation const&) -> Simulation& = delete;
    auto operator=(Simulation&&) -> Simulation& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> FSpinnerReadView { return {entities.get_const_view()}; }
    void set_config(FSpinnerSimulationConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_entity_registry() const -> FTestEntityRegistry const& { return entity_registry; }
    auto get_laser_simulation() const -> ml::test_lasers::Simulation const& {
        return laser_simulation;
    }

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;

    float entity_radius{0.f};
  private:
    /* **************************************** */
    // Simulation phases
    /* **************************************** */
    void begin_play();
    void update_timers(float dt);
    void move(float dt);
    void queue_commands();
    void end_tick();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    void spawn_instances(ml::simulation::Vectors3fConstView new_locations,
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
    friend struct ::FLevelSimulation;

    friend struct FSpinnerSpawnTestAccess;

    FSpinnerSimulationConfig config{};
    std::int16_t cooldown_restart_ticks_{};
    std::int16_t cooldown_cleaner_{};
    FSimulationClock const& simulation_clock;
    FTestEntityRegistry& entity_registry;
    ml::test_lasers::Simulation& laser_simulation;
    std::pmr::memory_resource& frame_memory_resource;
    EntityData entities{};
};
} // namespace ml::test_tube_spinners
