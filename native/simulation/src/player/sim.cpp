#include "ioj/sim/player/sim.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <ioj/sim/profiling.h>
#include <ioj/sim/ship_health.h>
#include <ioj/sim/transform3d.h>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/vector2d.h>
#include <span>

#include <algorithm>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/spatial_query_manager.h>

#include <array>
#include <limits>
#include <utility>

namespace ioj::sim::player::player_movement {
auto interpolate_to(double const current, double const target, float const dt, double const speed)
    -> double {
    if (speed <= 0.f) {
        return target;
    }
    auto const distance{target - current};
    if (distance * distance < 1.e-8) {
        return target;
    }
    return current + distance * std::clamp(static_cast<double>(dt) * speed, 0.0, 1.0);
}
}
namespace ioj::sim::player {
/* **************************************** */
// Construction and configuration
/* **************************************** */
void Sim::configure(PlayerSpawnData const& spawn) noexcept {
    config = spawn.config;
    team = spawn.team;

    movement_state_.transform = spawn.transform;
    movement_state_.body_transform = spawn.body_transform;
    left_socket = spawn.left_socket;
    right_socket = spawn.right_socket;
    middle_socket = spawn.middle_socket;
    flight_mode = spawn.flight_mode;
    control_mode = spawn.control_mode;
    laser_mode = spawn.laser_mode;
    laser_fire_rate = spawn.laser_fire_rate;
    health = spawn.health;
}
void Sim::set_config(PlayerSimConfig const& new_config) noexcept {
    config = new_config;
}

Sim::Sim(SimClock const& clock,
         EntityLedger& ledger,
         CombatEvents const& combat_events,
         SpatialQueryManager const& in_spatial_query_manager,
         lasers::Sim& in_lasers)
    : ledger_{ledger}
    , combat_events_{combat_events}
    , spatial_query_manager{in_spatial_query_manager}
    , lasers{in_lasers}
    , simulation_clock{clock} {}

/* **************************************** */
// Tick phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::begin_play");

    movement_state_.velocity = ml::Vector3d{};
    movement_state_.thrust_energy = config.thrust_energy_max;

    set_laser_mode(LaserFiringState::idle);
    set_laser_fire_rate(laser_fire_rate);

    configure_speed_sampling();
    set_boost_brake_state(player::BoostBrakeState::None);

    register_entity();
    health.clamp_to_max();
}

void Sim::prepare_tick(float const dt) {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::prepare_tick");

    laser_shot_cooldown -= dt;
    movement_state_.time_since_rotation_input += dt;

    if (speed_sampling_enabled) {
        --speed_sample_ticks_remaining;
        if (speed_sample_ticks_remaining <= 0) {
            sample_speed();
            speed_sample_ticks_remaining = speed_sample_tick_period;
        }
    }
}

void Sim::think(float const dt) {
    planned_movement_ = movement_state_;
    auto& movement{planned_movement_};
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::think");

    update_boost_brake(dt, movement);
    update_rotation(dt, movement);
    update_body_orientation(dt, movement);
    integrate_velocity(dt, movement);

    auto adjustment_input{planar_movement_direction};
    auto lateral_adjustment_speed{config.lateral_adjustment_speed};
    auto vertical_adjustment_speed{config.vertical_adjustment_speed};
    if (flight_mode == SpaceShipFlightMode::PlanarVelocity) {
        adjustment_input = sampling ? ml::Vector2d{} : adjustment_input.clamped_to_max_size(1.f);
        lateral_adjustment_speed = config.planar_lateral_trim_speed;
        vertical_adjustment_speed = config.planar_vertical_trim_speed;
    }

    auto const lateral_speed{adjustment_input.x * lateral_adjustment_speed};
    auto const vertical_speed{adjustment_input.y * vertical_adjustment_speed};
    auto const local_adjustment{ml::Vector3d{0.f, lateral_speed, vertical_speed}};
    movement.velocity += movement.transform.transform_vector_no_scale(local_adjustment);
    movement.transform.location += movement.velocity * dt;
}

void Sim::apply_movement() {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::apply_movement");
    movement_state_ = planned_movement_;
    if (std::exchange(fire_requested_, false)) {
        materialize_fire_command();
    }
}

void Sim::generate_fire_commands() {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::generate_fire_commands");
    update_laser_firing();
}

void Sim::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::resolve_damage_events");

    auto const damage_events{combat_events_.events_for(EntityType::PlayerShip)};
    auto const original_health{health.health};
    EntityUniqueId killer{};
    auto const damage_count{damage_events.num()};
    for (std::int32_t event_index{}; event_index < damage_count; ++event_index) {
        auto const element{static_cast<std::size_t>(event_index)};
        assert(damage_events.damaged_entities[element] == unique_entity_id);
        if (is_dead(health.health)) {
            continue;
        }

        auto const requested_damage{damage_events.damage_amounts[element]};
        assert(requested_damage >= 0);
        if (requested_damage == 0) {
            continue;
        }
        auto const was_alive{is_alive(health.health)};
        auto const applied_damage{std::min(health.health, requested_damage)};
        health.health -= requested_damage;
        ledger_.record_damage(unique_entity_id, damage_events.instigators[element], applied_damage);
        if (was_alive && is_dead(health.health)) {
            killer = damage_events.instigators[element];
        }
    }

    if (is_alive(original_health) && is_dead(health.health)) {
        die(killer);
    }
}

/* **************************************** */
// Identity and accounting
/* **************************************** */
void Sim::register_entity() {
    unique_entity_id = ledger_.record_spawn(EntityType::PlayerShip, team, health.is_alive());
}
void Sim::set_team(Team const new_team) noexcept {
    team = new_team;
    if (unique_entity_id.is_valid()) {
        ledger_.record_status(unique_entity_id, team, health.is_alive());
    }
}

/* **************************************** */
// Movement
/* **************************************** */
void Sim::integrate_velocity(float const dt, MovementState& movement) {
    switch (flight_mode) {
        case SpaceShipFlightMode::ForwardSpeed: {
            auto const new_speed{movement.forward_flight_model.update(dt)};
            movement.velocity = movement.transform.forward() * new_speed;
            break;
        }
        case SpaceShipFlightMode::PlanarVelocity: {
            movement.planar_velocity = movement.planar_flight_model.update(dt);
            movement.planar_boost_speed = movement.planar_boost_flight_model.update(dt);
            movement.velocity = movement.planar_velocity +
                                movement.transform.forward() * movement.planar_boost_speed;
            break;
        }
    }
}

void Sim::update_rotation(float const dt, MovementState& movement) {
    auto const rotation_step{config.rotation_speed * dt};
    if (rotation_input != ml::Vector2d{} || !(std::abs(roll_input) <= 1.e-8f)) {
        auto const yaw_strength{std::abs(rotation_input.x)};
        auto const yaw_step{config.rotation_speed * yaw_strength * dt};
        auto const delta_rotation{Rotator3d{rotation_input.y * rotation_step,
                                            rotation_input.x * yaw_step,
                                            roll_input * rotation_step}};
        movement.transform.rotation = movement.transform.rotation * to_quaternion(delta_rotation);
        movement.transform.rotation.normalize();
        movement.time_since_rotation_input = 0.f;
        return;
    }

    if (movement.time_since_rotation_input >= config.auto_level_roll_delay) {
        auto const rotation{movement.transform.rotator()};
        auto const roll{
            player_movement::interpolate_to(rotation.roll, 0.f, dt, config.auto_level_speed)};
        movement.transform.rotation = to_quaternion(Rotator3d{rotation.pitch, rotation.yaw, roll});
    }
}

void Sim::update_body_orientation(float const dt, MovementState& movement) {
    auto const current_rotation{movement.body_transform.rotator()};
    auto const target_pitch{rotation_input.y * config.pitch_angle_max};
    auto const new_pitch{player_movement::interpolate_to(
        current_rotation.pitch, target_pitch, dt, config.pitch_speed)};

    auto const target_yaw{rotation_input.x * config.yaw_angle_max};
    auto const new_yaw{
        player_movement::interpolate_to(current_rotation.yaw, target_yaw, dt, config.yaw_speed)};

    auto const turn_target{rotation_input.x * config.turn_bank_angle_max};
    auto const turn_speed{rotation_input.x * config.turn_bank_speed};
    auto const roll_speed{
        std::max(static_cast<double>(config.turn_bank_speed), std::abs(turn_speed))};
    auto const new_roll{
        player_movement::interpolate_to(current_rotation.roll, turn_target, dt, roll_speed)};
    movement.body_transform.rotation = to_quaternion(Rotator3d{new_pitch, new_yaw, new_roll});
}

void Sim::set_desired_planar_velocity(ml::Vector3d const desired_velocity) {
    target_local_planar_velocity = desired_velocity;

    auto const local_velocity{
        movement_state_.transform.inverse_transform_vector_no_scale(desired_velocity)};
    target_local_planar_velocity_scale = ml::Vector2d{local_velocity.y / config.cruise_speed,
                                                      local_velocity.x / config.cruise_speed};
    auto const response{config.speed_responses.accelerating_to_cruise};
    movement_state_.planar_flight_model.set_new_impulse(response.settling_time,
                                                        response.damping_ratio,
                                                        movement_state_.planar_velocity,
                                                        target_local_planar_velocity);
}

void Sim::set_boost_brake_state(BoostBrakeState const state) {
    set_boost_brake_state(state, movement_state_);
}

void Sim::set_boost_brake_state(BoostBrakeState const state, MovementState& movement) {
    if (state == player::BoostBrakeState::Boost && movement.boost_brake_state != state) {
        ++movement.boost_start_sequence;
    }

    auto const current_speed{static_cast<float>(movement.velocity.size())};
    auto const& speed_responses{config.speed_responses};
    if (state != player::BoostBrakeState::None && state != player::BoostBrakeState::Boost &&
        state != player::BoostBrakeState::Brake) {
        ml::log_error("Unhandled player boost/brake state.");
    }

    enum class SpeedResponseKind : std::uint8_t {
        AcceleratingToCruise,
        SlowingToCruise,
        Boost,
        Brake
    };
    struct ThrustTransition {
        float target_speed;
        float energy_change_rate;
        float planar_boost_target;
        SpeedResponseKind speed_response;
    };

    ThrustTransition transition{};
    switch (state) {
        case player::BoostBrakeState::Boost:
            transition = {config.boost_speed,
                          -(1.f / config.boost_depletion_time),
                          config.cruise_speed * config.boost_forward_speed_addition_multiplier,
                          SpeedResponseKind::Boost};
            break;
        case player::BoostBrakeState::Brake:
            transition = {config.brake_speed,
                          -(1.f / config.brake_depletion_time),
                          0.f,
                          SpeedResponseKind::Brake};
            break;
        default:
            transition = {config.cruise_speed,
                          1.f / config.thrust_recharge_time,
                          0.f,
                          config.cruise_speed < current_speed
                              ? SpeedResponseKind::SlowingToCruise
                              : SpeedResponseKind::AcceleratingToCruise};
            break;
    }
    std::array const responses{&speed_responses.accelerating_to_cruise,
                               &speed_responses.slowing_to_cruise,
                               &speed_responses.boost,
                               &speed_responses.brake};
    auto const response{*responses[static_cast<std::size_t>(transition.speed_response)]};
    movement.target_speed = transition.target_speed;
    movement.thrust_change_rate = transition.energy_change_rate;

    movement.forward_flight_model.set_new_impulse(
        response.settling_time, response.damping_ratio, current_speed, movement.target_speed);
    movement.planar_boost_flight_model.set_new_impulse(response.settling_time,
                                                       response.damping_ratio,
                                                       movement.planar_boost_speed,
                                                       transition.planar_boost_target);
    movement.boost_brake_state = state;
}

void Sim::update_boost_brake(float const dt, MovementState& movement) {
    auto const starting_energy{movement.thrust_energy};
    if (starting_energy <= 0.f) {
        set_boost_brake_state(player::BoostBrakeState::None, movement);
    }

    movement.thrust_energy += dt * movement.thrust_change_rate;
    movement.thrust_energy = std::clamp(movement.thrust_energy, 0.f, config.thrust_energy_max);
}

/* **************************************** */
// Flight controls
/* **************************************** */
void Sim::set_move_input(ml::Vector2d const input) noexcept {
    planar_movement_direction = input;
}

void Sim::set_lateral_move_input(float const input) noexcept {
    planar_movement_direction.x = input;
}

void Sim::set_vertical_move_input(float const input) noexcept {
    planar_movement_direction.y = input;
}

void Sim::set_ship_2d_control(ml::Vector2d const input) {
    if (!sampling) {
        return;
    }

    if (control_mode == SpaceShipControlMode::Velocity) {
        target_local_planar_velocity_scale = input;
    }
}

void Sim::set_ship_1d_control_x(float const input) {
    auto control{target_local_planar_velocity_scale};
    control.x = input;
    set_ship_2d_control(control);
}

void Sim::set_ship_1d_control_y(float const input) {
    auto control{target_local_planar_velocity_scale};
    control.y = input;
    set_ship_2d_control(control);
}

void Sim::select_next_control_mode() {
    control_mode =
        static_cast<SpaceShipControlMode>((static_cast<unsigned>(control_mode) + 1) %
                                          static_cast<unsigned>(SpaceShipControlMode::COUNT));
}

void Sim::select_previous_control_mode() {
    control_mode =
        static_cast<SpaceShipControlMode>((static_cast<unsigned>(control_mode) +
                                           static_cast<unsigned>(SpaceShipControlMode::COUNT) - 1) %
                                          static_cast<unsigned>(SpaceShipControlMode::COUNT));
}

void Sim::start_sampling() noexcept {
    if (sampling) {
        return;
    }
    planar_movement_direction = ml::Vector2d{};
    target_local_planar_velocity_scale = ml::Vector2d{};
    sampling = true;
}

void Sim::stop_sampling() {
    sampling = false;
    if (control_mode != SpaceShipControlMode::Velocity) {
        return;
    }

    auto const world_direction{
        movement_state_.transform.forward() * target_local_planar_velocity_scale.y +
        movement_state_.transform.right() * target_local_planar_velocity_scale.x};
    set_desired_planar_velocity(world_direction * config.cruise_speed);
}

void Sim::adjust_desired_forward_velocity(float const direction) {
    if (flight_mode != SpaceShipFlightMode::PlanarVelocity ||
        control_mode != SpaceShipControlMode::Velocity || sampling ||
        (std::abs(direction) <= 1.e-8f)) {
        return;
    }

    auto const forward{movement_state_.transform.forward()};
    auto const current_forward_velocity{ml::dot(target_local_planar_velocity, forward)};
    auto const adjustment_direction{direction > 0.f ? 1.f : -1.f};
    auto const adjustment{adjustment_direction * config.cruise_speed *
                          config.forward_velocity_trim_fraction};
    auto const desired_forward_velocity{std::clamp(current_forward_velocity + adjustment,
                                                   -static_cast<double>(config.cruise_speed),
                                                   static_cast<double>(config.cruise_speed))};
    auto const desired_velocity{target_local_planar_velocity +
                                forward * (desired_forward_velocity - current_forward_velocity)};
    set_desired_planar_velocity(desired_velocity);
}

void Sim::turn(ml::Vector2d const direction) noexcept {
    rotation_input = direction;
}

void Sim::start_boost() {
    if (energy_is_full() && movement_state_.boost_brake_state == player::BoostBrakeState::None) {
        set_boost_brake_state(player::BoostBrakeState::Boost);
    }
}

void Sim::stop_boost() {
    if (movement_state_.boost_brake_state == player::BoostBrakeState::Boost) {
        set_boost_brake_state(player::BoostBrakeState::None);
    }
}

void Sim::start_brake() {
    if (energy_is_full() && movement_state_.boost_brake_state == player::BoostBrakeState::None) {
        set_boost_brake_state(player::BoostBrakeState::Brake);
    }
}

void Sim::stop_brake() {
    if (movement_state_.boost_brake_state == player::BoostBrakeState::Brake) {
        set_boost_brake_state(player::BoostBrakeState::None);
    }
}

void Sim::roll(float const direction) noexcept {
    roll_input = std::clamp(direction, -1.f, 1.f);
}

void Sim::set_flight_mode(SpaceShipFlightMode const new_flight_mode) noexcept {
    flight_mode = new_flight_mode;
}

/* **************************************** */
// Weapons
/* **************************************** */
void Sim::set_lock_on_target(EntityUniqueId const target) noexcept {
    lock_on_target = target;
}

void Sim::set_laser_mode(LaserFiringState const mode) noexcept {
    laser_firing_mode = mode;
}

void Sim::update_laser_firing() {
    auto const cooldown_finished{laser_shot_cooldown <= 0.f};
    switch (laser_firing_mode) {
        case LaserFiringState::idle: {
            break;
        }
        case LaserFiringState::burst: {
            if (cooldown_finished) {
                fire_laser();
                laser_shot_cooldown = config.laser.fire_cooldown;

                if (lasers_fired_this_burst >= lasers_per_burst) {
                    laser_shot_cooldown = config.laser_lock_on_transition_delay;
                    set_laser_mode(LaserFiringState::lock_on_transition);
                }
            }
            break;
        }
        case LaserFiringState::lock_on_transition: {
            if (cooldown_finished) {
                set_laser_mode(LaserFiringState::lock_on_searching);
            }
            [[fallthrough]];
        }
        case LaserFiringState::lock_on_searching: {
            auto const middle{
                (middle_socket * planned_movement_.body_transform * planned_movement_.transform)};
            auto const start{middle.location};
            auto const end{start + middle.forward() * config.laser_lock_on_distance};
            auto const hit{spatial_query_manager.trace_closest(
                to_float(start), to_float(end), unique_entity_id)};

            if (hit.hit && hit.entity.is_valid()) {
                set_lock_on_target(hit.entity);
                set_laser_mode(LaserFiringState::lock_on_acquired);
            }
            break;
        }
        case LaserFiringState::lock_on_acquired: {
            break;
        }
    }
}

void Sim::start_fire_laser() {
    set_laser_mode(LaserFiringState::burst);
    lasers_fired_this_burst = 0;
    laser_shot_cooldown = 0.f;
    set_lock_on_target({});
}

void Sim::stop_fire_laser() {
    if (laser_firing_mode == LaserFiringState::lock_on_acquired) {
        set_lock_on_target({});
    }
    set_laser_mode(LaserFiringState::idle);
}

void Sim::fire_laser() {
    fire_requested_ = true;
    ++lasers_fired_this_burst;
    laser_shot_cooldown = config.laser.fire_cooldown;
}

void Sim::materialize_fire_command() {
    switch (laser_mode) {
        case ShipLaserMode::Single: {
            std::array<Transform3d, 1> const fire_points{
                (middle_socket * movement_state_.body_transform * movement_state_.transform)};
            fire_lasers_from(fire_points);
            break;
        }
        case ShipLaserMode::Double:
        case ShipLaserMode::Hyper: {
            std::array<Transform3d, 2> const fire_points{
                left_socket * movement_state_.body_transform * movement_state_.transform,
                right_socket * movement_state_.body_transform * movement_state_.transform};
            fire_lasers_from(fire_points);
            break;
        }
        default: {
            ml::fatal_error("Unhandled player laser mode.");
        }
    }
}

void Sim::fire_lasers_from(std::span<Transform3d const> const fire_points) {
    lasers::SingleAllocationLaserSpawnRequests new_lasers;
    auto const laser_count{static_cast<std::int32_t>(fire_points.size())};
    new_lasers.add_defaulted(laser_count);
    auto const laser_columns{new_lasers.get_view().columns()};

    for (std::int32_t i{0}; i < laser_count; ++i) {
        laser_columns.locations.set(i, to_float(fire_points[i].location));
        laser_columns.rotations.set(i, to_float(fire_points[i].rotator()));
        laser_columns.base_velocities.set(i, to_float(movement_state_.velocity));
    }

    std::ranges::fill(laser_columns.damages, config.laser.damage);
    std::ranges::fill(laser_columns.speeds, config.laser.projectile_speed);
    std::ranges::fill(laser_columns.max_distances, config.laser.max_distance);
    std::ranges::fill(laser_columns.sources, LaserSource{team, EntityType::PlayerShip});
    std::ranges::fill(laser_columns.instigator_ids, unique_entity_id);
    lasers.queue_laser_spawns(new_lasers.get_const_view().columns());
}

void Sim::upgrade_laser() noexcept {
    if (laser_mode == ShipLaserMode::Single) {
        laser_mode = ShipLaserMode::Double;
    } else if (laser_mode == ShipLaserMode::Double) {
        laser_mode = ShipLaserMode::Hyper;
    }
}

void Sim::select_next_laser_fire_rate() noexcept {
    set_laser_fire_rate(static_cast<ShipFireRate>((static_cast<unsigned>(laser_fire_rate) + 1) %
                                                  static_cast<unsigned>(ShipFireRate::COUNT)));
}

void Sim::select_previous_laser_fire_rate() noexcept {
    set_laser_fire_rate(static_cast<ShipFireRate>(
        (static_cast<unsigned>(laser_fire_rate) + static_cast<unsigned>(ShipFireRate::COUNT) - 1) %
        static_cast<unsigned>(ShipFireRate::COUNT)));
}

void Sim::set_laser_fire_rate(ShipFireRate const value) noexcept {
    laser_fire_rate = value;
    switch (laser_fire_rate) {
        case ShipFireRate::COUNT: {
            ml::fatal_error("Invalid player laser fire rate.");
        }
        case ShipFireRate::Single: {
            lasers_per_burst = 1;
            break;
        }
        case ShipFireRate::Burst3: {
            lasers_per_burst = 3;
            break;
        }
        case ShipFireRate::FullAuto: {
            lasers_per_burst = std::numeric_limits<decltype(lasers_per_burst)>::max();
            break;
        }
    }
}

/* **************************************** */
// Health and status
/* **************************************** */
void Sim::add_health(Health const added_health) {
    if (!health.is_alive()) {
        return;
    }
    set_health(health.health + added_health);
}

void Sim::set_health(Health const new_health, EntityUniqueId const killer) {
    if (new_health == health.health || !health.is_alive()) {
        return;
    }

    auto const was_alive{health.is_alive()};
    health.health = std::min(new_health, health.max_health);

    if (was_alive && !health.is_alive()) {
        die(killer);
    }
}

void Sim::die(EntityUniqueId const killer) {
    auto const reason{killer.is_valid() ? DeathReason::Combat : DeathReason::Unknown};
    ledger_.record_death(unique_entity_id, killer, reason);
    death_notification_pending = true;
}

auto Sim::consume_death_notification() noexcept -> bool {
    return std::exchange(death_notification_pending, false);
}

auto Sim::get_kills() const -> std::int32_t {
    return static_cast<std::int32_t>(ledger_.get_kills(unique_entity_id));
}

auto Sim::get_speed() const noexcept -> float {
    return static_cast<float>(movement_state_.velocity.size());
}

auto Sim::energy_is_full() const -> bool {
    return movement_state_.thrust_energy == config.thrust_energy_max;
}

auto Sim::get_energy() const -> float {
    assert(config.thrust_energy_max > 0.f);
    return movement_state_.thrust_energy / config.thrust_energy_max;
}

auto Sim::get_middle_socket() const -> Transform3d {
    return middle_socket * movement_state_.body_transform * movement_state_.transform;
}

auto Sim::get_laser_effective_range() const noexcept -> float {
    return std::max(config.laser.max_distance, 0.0f);
}

/* **************************************** */
// Diagnostics
/* **************************************** */
void Sim::sample_speed() {
    speed_samples[speed_sample_index] = {
        std::clamp(simulation_clock.get_simulation_time(), 0.0, 1e9),
        std::clamp(movement_state_.velocity.size(), 0.0, 100e3)};
    ++speed_sample_index;
    if (speed_sample_index >= speed_sample_max) {
        speed_sample_index = 0;
    }
}

void Sim::configure_speed_sampling() {
    static constexpr double sample_rate_hz{60.0};
    static constexpr double sample_window_seconds{5.0};
    auto const sample_tick_period{simulation_clock.frequency_to_tick_period(sample_rate_hz)};
    auto const sample_window_ticks{simulation_clock.duration_to_tick_period(sample_window_seconds)};
    assert(std::in_range<std::int32_t>(sample_tick_period));
    assert(sample_tick_period > 0);

    auto const sample_count{(sample_window_ticks + sample_tick_period - 1) / sample_tick_period};
    assert(std::in_range<std::int32_t>(sample_count));

    speed_sample_index = 0;
    speed_sample_max = static_cast<std::int32_t>(sample_count);
    speed_sample_tick_period = static_cast<std::int32_t>(sample_tick_period);
    speed_sample_ticks_remaining = speed_sample_tick_period;
    speed_samples.assign(static_cast<std::size_t>(speed_sample_max), ml::Vector2d{});
}
} // namespace player
