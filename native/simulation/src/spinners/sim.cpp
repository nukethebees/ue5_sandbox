#include "ioj/sim/spinners/sim.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <sandbox/core/countdown.h>
#include <sandbox/core/frame_array.h>
#include <sandbox/core/tick_countdown.h>
#include <span>
#include <utility>
#include <vector>

#include <ioj/sim/entity_registry.h>
#include <ioj/sim/lasers/frame_scratch.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_config.h>

namespace ioj::sim::spinners {

/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(SpinnerSimConfig const& new_config) noexcept {
    config = new_config;
}
Sim::Sim(SimClock const& clock,
         EntityRegistry& in_entity_registry,
         lasers::Sim& in_laser_simulation,
         std::pmr::memory_resource& in_frame_memory_resource) noexcept
    : simulation_clock{clock}
    , entity_registry{in_entity_registry}
    , laser_simulation{in_laser_simulation}
    , frame_memory_resource{in_frame_memory_resource} {}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("Sandbox::spinners::Sim::begin_play");

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    assert(cooldown_tick_period >= 0 && std::in_range<std::int16_t>(cooldown_tick_period));
    cooldown_restart_ticks_ = static_cast<std::int16_t>(cooldown_tick_period);
    cooldown_cleaner_ = 0;
    validate_array_sizes();
}
void Sim::prepare_tick(float const) {
    SANDBOX_PROFILE_SCOPE("Sandbox::spinners::Sim::prepare_tick");

    ml::tick_countdowns<std::int16_t>(
        entities.get_view().laser_cooldowns(), cooldown_cleaner_, 16384);
}
void Sim::think(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::spinners::Sim::think");

    planned_yaw_delta_ = dt * config.yaw_rotation_speed_degrees;
}
void Sim::apply_movement() {
    SANDBOX_PROFILE_SCOPE("Sandbox::spinners::Sim::apply_movement");
    for (auto& yaw : entities.get_view().yaws()) {
        yaw += planned_yaw_delta_;
    }
}
void Sim::generate_fire_commands() {
    SANDBOX_PROFILE_SCOPE("Sandbox::spinners::Sim::generate_fire_commands");

    fire_lasers();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("Sandbox::spinners::Sim::finish_action");
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}

/* **************************************** */
// Spawning
/* **************************************** */
auto Sim::spawn_instances(Vectors3fConstView const new_locations,
                          std::span<float const> const new_yaws,
                          std::span<std::int32_t const> const new_fire_point_indices)
    -> std::span<RegistryEntityHandle const> {
    SANDBOX_PROFILE_SCOPE("Sandbox::spinners::Sim::spawn_instances");

    auto const n{new_locations.num()};

    assert(new_yaws.size() == static_cast<std::size_t>(n));
    assert(new_fire_point_indices.size() == static_cast<std::size_t>(n));

    entities.add_uninitialised(n);
    auto const appended{entities.right(n).columns()};
    for (std::int32_t i{}; i < n; ++i) {
        appended.handles[i] = {};
        appended.locations.set(i, new_locations[i]);
        appended.yaws[i] = new_yaws[i];
        appended.laser_cooldowns[i] = 0;
        appended.next_fire_point_indices[i] = new_fire_point_indices[i];
    }

    entities.get_const_view().columns().validate_array_sizes();

    SingleAllocationRegistryEntityData entity_data;
    entity_data.add_uninitialised(n);
    auto const entity_columns{entity_data.get_view().columns()};
    for (std::int32_t i{}; i < n; ++i) {
        entity_columns.locations.set(i, new_locations[i]);
        entity_columns.rotations.set(i, {.pitch = 0.f, .yaw = new_yaws[i], .roll = 0.f});
    }
    entity_columns.velocities.each_column(
        [](auto const column) { std::ranges::fill(column, 0.f); });
    std::ranges::fill(entity_columns.healths, 1000000);
    std::ranges::fill(entity_columns.teams, Team::White);
    std::ranges::fill(entity_columns.entity_types, EntityType::TubeSpinner);
    entity_columns.validate_array_sizes();

    auto new_entities{entity_registry.add_entities(entity_data.get_const_view().columns())};

    for (std::int32_t i{0}; i < n; ++i) {
        appended.handles[i] = new_entities.get_handle(i);
    }

    validate_array_sizes();
    return appended.handles;
}

/* **************************************** */
// Firing
/* **************************************** */
void Sim::fire_lasers() {
    SANDBOX_PROFILE_SCOPE("Sandbox::spinners::Sim::fire_lasers");

    if (config.fire_point_offsets.empty()) {
        return;
    }

    auto const count{get_num_instances()};
    auto const entity_columns{entities.get_view().columns()};
    lasers::FrameSpawnRequests new_lasers{&frame_memory_resource};
    ml::FrameArray<std::int32_t> ready_indices{&frame_memory_resource};
    ready_indices.reserve(count);
    auto cooldowns{ml::TickCountdownView<std::int16_t>{entity_columns.laser_cooldowns,
                                                       cooldown_restart_ticks_}};
    for (std::int32_t index{}; index < count; ++index) {
        if (cooldowns.try_consume(static_cast<std::size_t>(index))) {
            ready_indices.add(index);
        }
    }

    auto const ready_count{ready_indices.num()};
    auto const fire_point_count{static_cast<std::int32_t>(config.fire_point_offsets.size())};
    new_lasers.set_num(ready_count);
    for (std::int32_t request_index{}; request_index < ready_count; ++request_index) {
        auto const index{ready_indices[request_index]};
        auto const element{static_cast<std::size_t>(index)};
        auto& next_fire_point{entity_columns.next_fire_point_indices[element]};
        assert(next_fire_point >= 0 && next_fire_point < fire_point_count);
        auto const& fire_point{
            config.fire_point_offsets[static_cast<std::size_t>(next_fire_point)]};
        new_lasers.locations.set(request_index,
                                 entity_columns.locations[index] + fire_point.location);
        new_lasers.rotations.set(
            request_index,
            {fire_point.rotation.pitch,
             fire_point.rotation.yaw + (entity_columns.yaws[element] + planned_yaw_delta_),
             fire_point.rotation.roll});
        new_lasers.base_velocities.set(request_index, HMM_V3(0.f, 0.f, 0.f));
        new_lasers.damages[request_index] = config.laser.damage;
        new_lasers.speeds[request_index] = config.laser.projectile_speed;
        new_lasers.max_distances[request_index] = config.laser.max_distance;
        new_lasers.instigator_handles[request_index] = entity_columns.handles[element];
        new_lasers.sources[request_index] = {Team::White, EntityType::TubeSpinner};
        next_fire_point = (next_fire_point + 1) % fire_point_count;
    }

    laser_simulation.queue_laser_spawns(new_lasers.get_const_view());
}

/* **************************************** */
// Checks
/* **************************************** */
void Sim::validate_array_sizes() const {
    entities.get_const_view().columns().validate_array_sizes();
}
} // namespace spinners
