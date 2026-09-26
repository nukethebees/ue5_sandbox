#include "ioj/sim/spinners/sim.h"

#include <ioj/sim/column_math.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/lasers/frame_scratch.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_config.h>

#include <sandbox/core/countdown.h>
#include <sandbox/core/frame_array.h>
#include <sandbox/core/tick_countdown.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ioj::sim::spinners {

/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(SpinnerSimConfig const& new_config) noexcept {
    config = new_config;
}
Sim::Sim(SimClock const& clock, EntityLedger& ledger, lasers::Sim& in_laser_simulation) noexcept
    : simulation_clock{clock}
    , ledger_{ledger}
    , laser_simulation{in_laser_simulation} {}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("spinners::Sim::begin_play");

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    assert(cooldown_tick_period >= 0 && std::in_range<std::int16_t>(cooldown_tick_period));
    cooldown_restart_ticks_ = static_cast<std::int16_t>(cooldown_tick_period);
    cooldown_cleaner_ = 0;
}
void Sim::prepare_tick(float const) {
    SANDBOX_PROFILE_SCOPE("spinners::Sim::prepare_tick");

    ml::tick_countdowns<std::int16_t>(
        entities.get_view().laser_cooldowns(), cooldown_cleaner_, 16384);
}
void Sim::think(float const dt) {
    SANDBOX_PROFILE_SCOPE("spinners::Sim::think");

    planned_yaw_delta_ = dt * config.yaw_rotation_speed_degrees;
}
void Sim::apply_movement(ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("spinners::Sim::apply_movement");
    for (auto& yaw : entities.get_view().yaws()) {
        yaw += planned_yaw_delta_;
    }
    materialize_fire_commands(scratch);
}
void Sim::generate_fire_commands() {
    SANDBOX_PROFILE_SCOPE("spinners::Sim::generate_fire_commands");

    fire_lasers();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("spinners::Sim::finish_action");
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}

/* **************************************** */
// Firing
/* **************************************** */
void Sim::fire_lasers() {
    SANDBOX_PROFILE_SCOPE("spinners::Sim::fire_lasers");

    if (config.fire_point_offsets.empty()) {
        return;
    }

    auto const count{get_num_instances()};
    auto const entity_columns{entities.get_view()};
    pending_fire_indices_.clear();
    pending_fire_indices_.reserve(count);
    auto cooldowns{ml::TickCountdownView<std::int16_t>{entity_columns.laser_cooldowns(),
                                                       cooldown_restart_ticks_}};
    for (std::int32_t index{}; index < count; ++index) {
        if (cooldowns.try_consume(static_cast<std::size_t>(index))) {
            pending_fire_indices_.push_back(index);
        }
    }
}
void Sim::materialize_fire_commands(ml::FrameScratch& scratch) {
    auto const entity_columns{entities.get_view()};
    lasers::FrameSpawnRequests new_lasers{scratch};
    auto const ready_count{static_cast<std::int32_t>(pending_fire_indices_.size())};
    auto const fire_point_count{static_cast<std::int32_t>(config.fire_point_offsets.size())};
    new_lasers.set_num(ready_count);
    auto const next_fire_points{entity_columns.next_fire_point_indices()};
    auto const locations{entity_columns.view_locations()};
    auto const yaws{entity_columns.yaws()};
    auto const entity_ids{entity_columns.entity_ids()};

    auto const laser_locations{new_lasers.view_locations().get_view()};
    auto const laser_rotations{new_lasers.view_rotations().get_view()};
    auto const laser_base_velocities{new_lasers.view_base_velocities().get_view()};
    auto const laser_damages{new_lasers.damages()};
    auto const laser_speeds{new_lasers.speeds()};
    auto const laser_max_distances{new_lasers.max_distances()};
    auto const laser_instigator_ids{new_lasers.instigator_ids()};
    auto const laser_sources{new_lasers.sources()};

    for (std::int32_t request_index{}; request_index < ready_count; ++request_index) {
        auto const index{pending_fire_indices_[request_index]};
        auto const element{static_cast<std::size_t>(index)};
        auto& next_fire_point{next_fire_points[element]};
        assert(next_fire_point >= 0 && next_fire_point < fire_point_count);
        auto const& fire_point{
            config.fire_point_offsets[static_cast<std::size_t>(next_fire_point)]};
        set_vector(
            laser_locations, request_index, vector_at(locations, index) + fire_point.location);
        laser_rotations.set(request_index,
                            {fire_point.rotation.pitch,
                             fire_point.rotation.yaw + yaws[element],
                             fire_point.rotation.roll});
        set_vector(laser_base_velocities, request_index, HMM_V3(0.f, 0.f, 0.f));
        laser_damages[request_index] = config.laser.damage;
        laser_speeds[request_index] = config.laser.projectile_speed;
        laser_max_distances[request_index] = config.laser.max_distance;
        laser_instigator_ids[request_index] = entity_ids[element];
        laser_sources[request_index] = {Team::White, EntityType::TubeSpinner};
        next_fire_point = (next_fire_point + 1) % fire_point_count;
    }

    laser_simulation.queue_laser_spawns(new_lasers);
    pending_fire_indices_.clear();
}

/* **************************************** */
// Checks
/* **************************************** */
} // namespace spinners
