#include "sandbox/simulation/defences/spinners/TestTubeSpinnersSimulation.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <sandbox/core/countdown.h>
#include <sandbox/core/tick_countdown.h>
#include <span>
#include <utility>
#include <vector>

#include <sandbox/simulation/combat/lasers/TestLasersFrameScratch.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/simulation/LevelSimulationConfig.h>

namespace ml::test_tube_spinners {

/* **************************************** */
// Configuration
/* **************************************** */
void Simulation::set_config(FSpinnerSimulationConfig const& new_config) noexcept {
    config = new_config;
}
Simulation::Simulation(FSimulationClock const& clock,
                       FTestEntityRegistry& in_entity_registry,
                       ml::test_lasers::Simulation& in_laser_simulation,
                       std::pmr::memory_resource& in_frame_memory_resource) noexcept
    : simulation_clock{clock}
    , entity_registry{in_entity_registry}
    , laser_simulation{in_laser_simulation}
    , frame_memory_resource{in_frame_memory_resource} {}

/* **************************************** */
// Simulation phases
/* **************************************** */
void Simulation::begin_play() {
    assert(entity_radius > 0.f);

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    assert(cooldown_tick_period >= 0 && std::in_range<std::int16_t>(cooldown_tick_period));
    cooldown_restart_ticks_ = static_cast<std::int16_t>(cooldown_tick_period);
    cooldown_cleaner_ = 0;
    validate_array_sizes();
}
void Simulation::update_timers(float const) {

    ml::tick_countdowns<std::int16_t>(entities.laser_cooldowns, cooldown_cleaner_, 16384);
}
void Simulation::move(float const dt) {

    rotate_instances(dt);
}
void Simulation::queue_commands() {

    fire_lasers();
}
void Simulation::end_tick() {}

/* **************************************** */
// Accessors
/* **************************************** */
auto Simulation::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}

/* **************************************** */
// Spawning
/* **************************************** */
void Simulation::spawn_instances(ml::simulation::Vectors3fConstView const new_locations,
                                 std::span<float const> const new_yaws,
                                 std::span<std::int32_t const> const new_fire_point_indices) {

    auto const n{new_locations.num()};

    assert(new_yaws.size() == static_cast<std::size_t>(n));
    assert(new_fire_point_indices.size() == static_cast<std::size_t>(n));

    entities.add_uninitialised(n);
    auto appended{entities.right(n)};
    for (std::int32_t i{}; i < n; ++i) {
        appended.handles[i] = {};
        appended.locations.set(i, new_locations[i]);
        appended.yaws[i] = new_yaws[i];
        appended.laser_cooldowns[i] = 0;
        appended.next_fire_point_indices[i] = new_fire_point_indices[i];
    }

    entities.validate_array_sizes();

    ml::simulation::RegistryEntityData entity_data;
    entity_data.add_uninitialised(n);
    for (std::int32_t i{}; i < n; ++i) {
        entity_data.locations.set(i, new_locations[i]);
        entity_data.rotations.set(i, {.pitch = 0.f, .yaw = new_yaws[i], .roll = 0.f});
    }
    entity_data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    std::ranges::fill(entity_data.radii, entity_radius);
    std::ranges::fill(entity_data.healths, 1000000);
    std::ranges::fill(entity_data.teams, ml::simulation::Team::White);
    std::ranges::fill(entity_data.entity_types, ml::simulation::EntityType::TubeSpinner);
    std::ranges::fill(entity_data.alive, std::uint8_t{1});
    entity_data.validate_array_sizes();

    auto new_entities{entity_registry.add_entities(entity_data.get_const_view())};

    for (std::int32_t i{0}; i < n; ++i) {
        appended.handles[i] = new_entities.get_handle(i);
    }

    validate_array_sizes();
}

/* **************************************** */
// Movement
/* **************************************** */
void Simulation::rotate_instances(float const dt) {

    auto const speed{config.yaw_rotation_speed_degrees};
    auto const delta_yaw_degrees{dt * speed};

    for (auto& yaw : entities.yaws) {
        yaw += delta_yaw_degrees;
    }
}

/* **************************************** */
// Firing
/* **************************************** */
void Simulation::fire_lasers() {

    if (config.fire_point_offsets.empty()) {
        return;
    }

    auto const count{static_cast<std::size_t>(get_num_instances())};
    ml::simulation::lasers::FrameSpawnRequests new_lasers{&frame_memory_resource};
    ml::simulation::spinners::fire_lasers(
        {.locations = entities.locations.get_const_view(),
         .yaws = {entities.yaws.data(), count},
         .handles = {entities.handles.data(), count},
         .next_fire_point_indices = {entities.next_fire_point_indices.data(), count},
         .cooldowns = ml::TickCountdownView<std::int16_t>{entities.laser_cooldowns,
                                                          cooldown_restart_ticks_}},
        config.fire_point_offsets,
        {.damage = config.laser.damage,
         .speed = config.laser.projectile_speed,
         .maximum_distance = config.laser.max_distance},
        frame_memory_resource,
        new_lasers);
    laser_simulation.queue_laser_spawns(new_lasers.get_const_view());
}

/* **************************************** */
// Checks
/* **************************************** */
void Simulation::validate_array_sizes() const {
    entities.validate_array_sizes();
}
} // namespace ml::test_tube_spinners
