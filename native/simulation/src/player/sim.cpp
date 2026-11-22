#include "ioj/sim/player/sim.h"

#include <ioj/column_math.h>
#include <ioj/diagnostics.h>
#include <ioj/frame_array.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/damage_queue.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/ship_health.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/phase_checks.h>
#include <ioj/sim/profiling.h>
#include <ioj/transform3d.h>
#include <ioj/vector_value_math.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
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
// Level initialisation
void Sim::initialise(PlayerSpawnData const& spawn,
                     EntityTables& tables,
                     EntityLedger& ledger,
                     SimClock const& clock) {
    config_ = spawn.config;
    team = spawn.team;

    state_.physical.transform = spawn.transform;
    state_.presentation.body_transform = spawn.body_transform;
    left_socket = spawn.left_socket;
    right_socket = spawn.right_socket;
    middle_socket = spawn.middle_socket;
    laser_mode = spawn.laser_mode;
    laser_trigger_mode = spawn.laser_trigger_mode;
    max_health_ = spawn.health.max_health;
    ledger.record_spawns(EntityType::PlayerShip, team, spawn.health.health, {&unique_entity_id, 1});
    auto const initial_health{spawn.health.health};
    tables.health.initialise_entities<EntityType::PlayerShip>({&unique_entity_id, 1},
                                                              initial_health);

    SANDBOX_PROFILE_SCOPE("PlayerShipSim::initialise");

    state_.physical.velocity = Vector3d{};
    state_.resources.thrust_energy = config_.thrust_energy_max;
    state_.controller = {};
    state_.controller.roll_angle = static_cast<float>(spawn.transform.rotator().roll);
    reset_speed_responses(state_);

    set_laser_mode(LaserFiringState::idle);
    set_laser_trigger_mode(laser_trigger_mode);

    configure_speed_sampling(clock);
    planned_state_ = state_;
}
void Sim::set_config(PlayerSimConfig const& new_config) noexcept {
    config_ = new_config;
    reset_speed_responses(state_);
}

Sim::Sim(std::pmr::memory_resource& memory)
    : input_queue_{std::pmr::vector<PlayerInputKind>{&memory}}
    , health_changes_{&memory} {}

/* **************************************** */
// Tick phases
/* **************************************** */
// Preparation
void Sim::update_entity_lookup_table(EntityTables& tables, [[maybe_unused]] SimClock const& clock) {
    assert(is_preparation_mutation_allowed(clock));

    tables.publish<EntityType::PlayerShip>({&unique_entity_id, 1}, {&team, 1}, max_health_);
}
void Sim::prepare_tick(float const dt, EntityTables& tables, SimClock const& clock) {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::prepare_tick");

    laser_shot_cooldown -= dt;
    state_.controller.elapsed_since_spawn += dt;

    if (speed_sampling_enabled) {
        --speed_sample_ticks_remaining;
        if (speed_sample_ticks_remaining <= 0) {
            sample_speed(clock);
            speed_sample_ticks_remaining = speed_sample_tick_period;
        }
    }
    update_entity_lookup_table(tables, clock);
}

// Thinking
void Sim::think(float const dt, PlayerEvents& events) {
    planned_state_ = state_;
    auto& state{planned_state_};
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::think");

    apply_inputs(state, events);
    update_boost_brake(dt, state, events);
    update_body_orientation(dt, state);
    update_movement(dt, state);
}
void Sim::apply_inputs(PlayerSimulationState& state, PlayerEvents& events) {
    for (auto const input : input_queue_.inputs) {
        switch (input) {
            case PlayerInputKind::BoostPressed:
                flight_intent_.boost_held = true;
                break;
            case PlayerInputKind::BoostReleased:
                flight_intent_.boost_held = false;
                break;
            case PlayerInputKind::BrakePressed:
                flight_intent_.brake_held = true;
                break;
            case PlayerInputKind::BrakeReleased:
                flight_intent_.brake_held = false;
                break;
            case PlayerInputKind::EmergencyBrakePressed:
                flight_intent_.emergency_brake_held = true;
                break;
            case PlayerInputKind::EmergencyBrakeReleased:
                flight_intent_.emergency_brake_held = false;
                break;
            case PlayerInputKind::FirePressed:
                set_laser_mode(LaserFiringState::burst);
                lasers_fired_this_burst = 0;
                laser_shot_cooldown = 0.f;
                continue;
            case PlayerInputKind::FireReleased:
                set_laser_mode(LaserFiringState::idle);
                continue;
        }
        update_propulsion_action(state, events);
    }
    input_queue_.inputs.clear();
}
void Sim::generate_fire_commands() {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::generate_fire_commands");
    update_laser_firing();
}

// Action
void Sim::apply_movement(ActionInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::apply_movement");
    state_ = planned_state_;
    if (std::exchange(fire_requested_, false)) {
        materialize_fire_command(inputs);
        inputs.events.push_back(PlayerEventKind::ShotFired);
    }
}

// Resolution
void Sim::resolve_damage_events(ResolutionInputs const inputs) {

    SANDBOX_PROFILE_SCOPE("PlayerShipSim::resolve_damage_events");

    auto const damage_events{inputs.damage_queue.events_for(EntityType::PlayerShip)};
    auto& health_table{inputs.health_table};
    auto const original_health{health_table.health(unique_entity_id)};
    auto health{original_health};
    for (auto const delta : health_changes_) {
        if (is_dead(health)) {
            break;
        }
        health = std::min(Health{health.value() + delta.value()}, max_health_);
    }
    health_changes_.clear();
    EntityId killer{};
    auto const damage_count{damage_events.num()};
    auto const damage_amounts{damage_events.damage_amounts()};
    auto const instigators{damage_events.instigators()};
    for (std::uint32_t event_index{}; event_index < damage_count; ++event_index) {
        auto const element{static_cast<std::size_t>(event_index)};
        if (is_dead(health)) {
            continue;
        }

        auto const requested_damage{damage_amounts[element]};
        if (requested_damage == 0) {
            continue;
        }
        health.set_value(health.value() - requested_damage);
        if (is_dead(health)) {
            killer = instigators[element];
        }
    }

    health_table.set_health(unique_entity_id, health);

    if (sim::is_alive(original_health) && is_dead(health)) {
        inputs.deaths.add(killer.is_valid() ? DeathReason::Combat : DeathReason::Unknown,
                          unique_entity_id,
                          killer,
                          Vector3f{state_.physical.transform.location});
    }
}

/* **************************************** */
// Identity and accounting
/* **************************************** */
void Sim::set_team(Team const new_team,
                   HealthTable const& health_table,
                   EntityLedger& ledger) noexcept {
    team = new_team;
    if (unique_entity_id.is_valid()) {
        auto const health{health_table.health(unique_entity_id)};
        ledger.record_status({&unique_entity_id, 1}, {&team, 1}, {&health, 1});
    }
}

/* **************************************** */
// Movement
/* **************************************** */
auto Sim::target_speed() const noexcept -> float {
    switch (state_.controller.effective_action) {
        case BoostBrakeState::Boost:
            return config_.boost_speed;
        case BoostBrakeState::Brake:
        case BoostBrakeState::EmergencyBrake:
            return config_.brake_speed;
        default:
            return config_.cruise_speed;
    }
}

void Sim::reset_speed_responses(PlayerSimulationState& state) const noexcept {
    auto& controller{state.controller};
    auto const speed{
        static_cast<float>(dot(state.physical.velocity, state.physical.transform.forward()))};
    auto const target{controller.effective_action == BoostBrakeState::Boost ? config_.boost_speed
                                                                            : config_.cruise_speed};
    controller.speed_response.reset(
        config_.response_settling_time, config_.response_damping_ratio, speed, target);
    controller.brake_response.reset(
        config_.response_settling_time, config_.response_damping_ratio, 0.f, 1.f);
}

void Sim::update_movement(float const dt, PlayerSimulationState& state) const noexcept {
    auto& physical{state.physical};
    auto& controller{state.controller};
    auto const pitch{static_cast<double>(flight_intent_.pitch * config_.turn_rate) * dt};
    auto const yaw{static_cast<double>(flight_intent_.yaw * config_.turn_rate) * dt};
    physical.transform.rotation =
        physical.transform.rotation * to_quaternion(Rotator3d{pitch, yaw, 0.0});
    normalize(physical.transform.rotation);
    auto rotation{physical.transform.rotator()};
    if (controller.elapsed_since_spawn < config_.roll_level_delay) {
        controller.roll_angle = static_cast<float>(rotation.roll);
    } else {
        auto const step{config_.roll_level_rate * dt};
        controller.roll_angle =
            std::clamp(0.f, controller.roll_angle - step, controller.roll_angle + step);
        rotation.roll = controller.roll_angle;
    }
    physical.transform.rotation = to_quaternion(rotation);

    auto const action{controller.effective_action};
    auto const forward{physical.transform.forward()};
    auto const limit{action == BoostBrakeState::Boost ? config_.boost_speed.get()
                                                      : config_.cruise_speed.get()};
    if (action == BoostBrakeState::Brake || action == BoostBrakeState::EmergencyBrake) {
        auto const speed{static_cast<float>(size(physical.velocity))};
        if (speed > config_.brake_speed) {
            auto const engagement{std::clamp(controller.brake_response.update(dt), 0.f, 1.f)};
            auto const deceleration{action == BoostBrakeState::EmergencyBrake
                                        ? config_.emergency_brake_deceleration
                                        : config_.brake_deceleration};
            physical.velocity =
                physical.velocity *
                (std::max(config_.brake_speed.get(), speed - deceleration * engagement * dt) /
                 speed);
        }
    } else {
        auto const speed{std::clamp(controller.speed_response.update(dt), 0.f, limit)};
        physical.velocity +=
            forward * (speed - static_cast<float>(dot(physical.velocity, forward)));
    }
    physical.velocity = forward * std::min(size(physical.velocity), static_cast<double>(limit));
    physical.transform.location += physical.velocity * dt;
}

void Sim::update_body_orientation(float const dt, PlayerSimulationState& state) {
    auto& presentation{state.presentation};
    auto const current_rotation{presentation.body_transform.rotator()};
    auto const target_pitch{flight_intent_.pitch * config_.pitch_angle_max};
    auto const new_pitch{player_movement::interpolate_to(
        current_rotation.pitch, target_pitch, dt, config_.pitch_speed)};

    auto const target_yaw{flight_intent_.yaw * config_.yaw_angle_max};
    auto const new_yaw{
        player_movement::interpolate_to(current_rotation.yaw, target_yaw, dt, config_.yaw_speed)};

    auto const turn_target{flight_intent_.yaw * config_.turn_bank_angle_max};
    auto const turn_speed{flight_intent_.yaw * config_.turn_bank_speed};
    auto const roll_speed{std::max(config_.turn_bank_speed, std::abs(turn_speed))};
    auto const new_roll{
        player_movement::interpolate_to(current_rotation.roll, turn_target, dt, roll_speed)};
    presentation.body_transform.rotation = to_quaternion(Rotator3d{new_pitch, new_yaw, new_roll});
}

void Sim::update_boost_brake(float const dt, PlayerSimulationState& state, PlayerEvents& events) {
    auto& controller{state.controller};
    auto& resources{state.resources};
    update_propulsion_action(state, events);

    auto energy_change{config_.energy_recharge.get()};
    switch (controller.effective_action) {
        case BoostBrakeState::None:
            break;
        case BoostBrakeState::Brake:
            energy_change = -config_.brake_energy_drain;
            break;
        case BoostBrakeState::Boost:
            energy_change = -config_.boost_energy_drain;
            break;
        case BoostBrakeState::EmergencyBrake:
            energy_change = -config_.brake_energy_drain;
            break;
    }
    resources.thrust_change_rate = energy_change * config_.thrust_energy_max;
    resources.thrust_energy += dt * resources.thrust_change_rate;
    resources.thrust_energy =
        std::clamp<float>(resources.thrust_energy, 0.f, config_.thrust_energy_max);
    if (resources.thrust_energy <= 0.f) {
        update_propulsion_action(state, events);
    }
}

void Sim::update_propulsion_action(PlayerSimulationState& state, PlayerEvents& events) {
    auto const has_energy = [&state](float const drain) {
        return drain == 0.f || state.resources.thrust_energy > 0.f;
    };
    auto action{BoostBrakeState::None};
    if (flight_intent_.emergency_brake_held && has_energy(config_.brake_energy_drain)) {
        action = BoostBrakeState::EmergencyBrake;
    } else if (flight_intent_.brake_held && has_energy(config_.brake_energy_drain)) {
        action = BoostBrakeState::Brake;
    } else if ((flight_intent_.boost_held || flight_intent_.accelerator > 0.f) &&
               has_energy(config_.boost_energy_drain)) {
        action = BoostBrakeState::Boost;
    }

    if (action == BoostBrakeState::Boost &&
        state.controller.effective_action != BoostBrakeState::Boost) {
        events.push_back(PlayerEventKind::BoostStarted);
    } else if (action != BoostBrakeState::Boost &&
               state.controller.effective_action == BoostBrakeState::Boost) {
        events.push_back(PlayerEventKind::BoostStopped);
    }
    if (action != state.controller.effective_action) {
        state.controller.effective_action = action;
        reset_speed_responses(state);
    }
}

/* **************************************** */
// Flight controls
/* **************************************** */
void Sim::set_accelerator(float const input) noexcept {
    flight_intent_.accelerator = std::clamp(input, 0.f, 1.f);
}

void Sim::set_pitch_input(float const input) noexcept {
    flight_intent_.pitch = std::clamp(input, -1.f, 1.f);
}

void Sim::set_yaw_input(float const input) noexcept {
    flight_intent_.yaw = std::clamp(input, -1.f, 1.f);
}

void Sim::start_boost() {
    input_queue_.add(PlayerInputKind::BoostPressed);
}

void Sim::stop_boost() {
    input_queue_.add(PlayerInputKind::BoostReleased);
}

void Sim::start_brake() {
    input_queue_.add(PlayerInputKind::BrakePressed);
}

void Sim::start_emergency_brake() {
    input_queue_.add(PlayerInputKind::EmergencyBrakePressed);
}

void Sim::stop_emergency_brake() {
    input_queue_.add(PlayerInputKind::EmergencyBrakeReleased);
}

void Sim::stop_brake() {
    input_queue_.add(PlayerInputKind::BrakeReleased);
}

/* **************************************** */
// Weapons
/* **************************************** */

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
                laser_shot_cooldown = config_.laser.fire_cooldown;

                if (lasers_fired_this_burst >= lasers_per_burst) {
                    set_laser_mode(LaserFiringState::idle);
                }
            }
            break;
        }
    }
}

void Sim::start_fire_laser() {
    input_queue_.add(PlayerInputKind::FirePressed);
}

void Sim::stop_fire_laser() {
    input_queue_.add(PlayerInputKind::FireReleased);
}

void Sim::fire_laser() {
    fire_requested_ = true;
    ++lasers_fired_this_burst;
    laser_shot_cooldown = config_.laser.fire_cooldown;
}

void Sim::materialize_fire_command(ActionInputs const inputs) {
    switch (laser_mode) {
        case ShipLaserMode::Single: {
            std::array<Transform3d, 1> const fire_points{
                (middle_socket * state_.presentation.body_transform * state_.physical.transform)};
            fire_lasers_from(inputs, fire_points);
            break;
        }
        case ShipLaserMode::Double:
        case ShipLaserMode::Hyper: {
            std::array<Transform3d, 2> const fire_points{
                left_socket * state_.presentation.body_transform * state_.physical.transform,
                right_socket * state_.presentation.body_transform * state_.physical.transform};
            fire_lasers_from(inputs, fire_points);
            break;
        }
        default: {
            fatal_error("Unhandled player laser mode.");
        }
    }
}

void Sim::fire_lasers_from(ActionInputs const inputs,
                           std::span<Transform3d const> const fire_points) {
    auto const laser_count{static_cast<std::uint32_t>(fire_points.size())};
    auto const laser_columns{inputs.laser_simulation.append_spawn_requests(laser_count)};
    auto const locations{laser_columns.view_locations().resolve()};
    auto const rotations{laser_columns.view_rotations().resolve()};
    auto const base_velocities{laser_columns.view_base_velocities().resolve()};

    for (std::uint32_t i{}; i < laser_count; ++i) {
        set_vector(locations, i, Vector3f{fire_points[i].location});
        set_rotation(rotations, i, Rotator3f{fire_points[i].rotator()});
        set_vector(base_velocities, i, Vector3f{state_.physical.velocity});
    }

    std::ranges::fill(laser_columns.damages(), config_.laser.damage);
    std::ranges::fill(laser_columns.speeds(), config_.laser.projectile_speed);
    std::ranges::fill(laser_columns.max_distances(), config_.laser.max_distance);
    std::ranges::fill(laser_columns.sources(), LaserSource{team, EntityType::PlayerShip});
    std::ranges::fill(laser_columns.instigator_ids(), unique_entity_id);
}

void Sim::upgrade_laser() noexcept {
    if (laser_mode == ShipLaserMode::Single) {
        laser_mode = ShipLaserMode::Double;
    } else if (laser_mode == ShipLaserMode::Double) {
        laser_mode = ShipLaserMode::Hyper;
    }
}

void Sim::select_next_laser_trigger_mode() noexcept {
    set_laser_trigger_mode(static_cast<ShipTriggerMode>(
        (static_cast<unsigned>(laser_trigger_mode) + 1) % enum_count<ShipTriggerMode>()));
}

void Sim::select_previous_laser_trigger_mode() noexcept {
    set_laser_trigger_mode(static_cast<ShipTriggerMode>(
        (static_cast<unsigned>(laser_trigger_mode) + enum_count<ShipTriggerMode>() - 1) %
        enum_count<ShipTriggerMode>()));
}

void Sim::set_laser_trigger_mode(ShipTriggerMode const value) noexcept {
    laser_trigger_mode = value;
    switch (laser_trigger_mode) {
        default: {
            fatal_error("Invalid player laser trigger mode.");
        }
        case ShipTriggerMode::Single: {
            lasers_per_burst = 1;
            break;
        }
        case ShipTriggerMode::Burst3: {
            lasers_per_burst = 3;
            break;
        }
        case ShipTriggerMode::FullAuto: {
            lasers_per_burst = std::numeric_limits<decltype(lasers_per_burst)>::max();
            break;
        }
    }
}

/* **************************************** */
// Health and status
/* **************************************** */
void Sim::add_health(Health const delta) {
    health_changes_.push_back(delta);
}

auto Sim::health(HealthTable const& health_table) const -> ShipHealth {
    return {health_table.health(unique_entity_id), max_health_};
}

auto Sim::is_alive(HealthTable const& health_table) const -> bool {
    return unique_entity_id.is_valid() && sim::is_alive(health_table.health(unique_entity_id));
}

auto Sim::speed() const noexcept -> float {
    return static_cast<float>(size(state_.physical.velocity));
}

auto Sim::energy_is_full() const -> bool {
    return state_.resources.thrust_energy == config_.thrust_energy_max;
}

auto Sim::energy() const -> float {
    return state_.resources.thrust_energy / config_.thrust_energy_max;
}

auto Sim::get_middle_socket() const -> Transform3d {
    return middle_socket * state_.presentation.body_transform * state_.physical.transform;
}

auto Sim::laser_effective_range() const noexcept -> float {
    return config_.laser.max_distance;
}

/* **************************************** */
// Diagnostics
/* **************************************** */
void Sim::sample_speed(SimClock const& clock) {
    speed_samples[speed_sample_index] = {std::clamp(clock.simulation_time(), 0.0, 1e9),
                                         std::clamp(size(state_.physical.velocity), 0.0, 100e3)};
    ++speed_sample_index;
    if (speed_sample_index >= speed_sample_max) {
        speed_sample_index = 0;
    }
}

void Sim::configure_speed_sampling(SimClock const& clock) {
    static constexpr double sample_rate_hz{60.0};
    static constexpr double sample_window_seconds{5.0};
    std::int32_t const sample_tick_period{
        clock.frequency_to_tick_period<std::int32_t>(Frequency::make(sample_rate_hz))};
    SimTick const sample_window_ticks{
        clock.duration_to_tick_period(Duration::make(sample_window_seconds))};

    auto const sample_count{(sample_window_ticks.value() + sample_tick_period - 1) /
                            sample_tick_period};
    assert(std::in_range<std::uint32_t>(sample_count));

    speed_sample_index = 0;
    speed_sample_max = static_cast<std::uint32_t>(sample_count);
    speed_sample_tick_period = static_cast<std::int32_t>(sample_tick_period);
    speed_sample_ticks_remaining = speed_sample_tick_period;
    speed_samples.assign(static_cast<std::size_t>(speed_sample_max), Vector2d{});
}
} // namespace player
