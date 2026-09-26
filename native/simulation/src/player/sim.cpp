#include "ioj/sim/player/sim.h"

#include <ioj/sim/column_math.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/player/flight_model_evaluator.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/ship_health.h>
#include <ioj/sim/spatial_query_manager.h>
#include <ioj/sim/transform3d.h>

#include <sandbox/core/diagnostics.h>
#include <sandbox/core/vector2d.h>
#include <sandbox/core/vector3d.h>

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

auto target_speed_bound(TranslationDriveConfig const& drive, float const direction) -> float {
    auto const requested{direction >= 0.f ? drive.positive_target_speed
                                          : drive.negative_target_speed};
    auto const limit{direction >= 0.f ? drive.positive_speed_limit : drive.negative_speed_limit};
    return std::min(requested, limit);
}

auto target_speed_from_input(TranslationDriveConfig const& drive, float const input) -> float {
    return input * target_speed_bound(drive, input);
}
}
namespace ioj::sim::player {
/* **************************************** */
// Construction and configuration
/* **************************************** */
void Sim::configure(PlayerSpawnData const& spawn) noexcept {
    config = spawn.config;
    team = spawn.team;

    state_.physical.transform = spawn.transform;
    state_.presentation.body_transform = spawn.body_transform;
    left_socket = spawn.left_socket;
    right_socket = spawn.right_socket;
    middle_socket = spawn.middle_socket;
    flight_models_ = spawn.flight_models;
    active_flight_model_slot_ = flight_models_.initial_slot;
    laser_mode = spawn.laser_mode;
    laser_fire_rate = spawn.laser_fire_rate;
    max_health_ = spawn.health.max_health;
    unique_entity_id = ledger_.record_spawn(EntityType::PlayerShip, team, spawn.health.is_alive());
    auto const initial_health{spawn.health.health};
    health_table_.add(std::span<EntityUniqueId const>{&unique_entity_id, 1},
                      std::span<Health const>{&initial_health, 1},
                      std::span<HealthIndex>{&health_index_, 1});
}
void Sim::set_config(PlayerSimConfig const& new_config) noexcept {
    config = new_config;
}

Sim::Sim(SimClock const& clock,
         EntityLedger& ledger,
         CombatEvents const& combat_events,
         HealthTable& health_table,
         SpatialQueryManager const& in_spatial_query_manager,
         lasers::Sim& in_lasers)
    : ledger_{ledger}
    , combat_events_{combat_events}
    , health_table_{health_table}
    , spatial_query_manager{in_spatial_query_manager}
    , lasers{in_lasers}
    , simulation_clock{clock} {}

/* **************************************** */
// Tick phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::begin_play");

    state_.physical.velocity = ml::Vector3d{};
    state_.resources.thrust_energy = config.thrust_energy_max;
    reset_flight_model_controller(state_, get_active_flight_model_config());

    set_laser_mode(LaserFiringState::idle);
    set_laser_fire_rate(laser_fire_rate);

    configure_speed_sampling();
    refresh_effective_action(state_);
    planned_state_ = state_;
}

void Sim::prepare_tick(float const dt) {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::prepare_tick");

    laser_shot_cooldown -= dt;
    state_.controller.time_since_rotation_input.x += dt;
    state_.controller.time_since_rotation_input.y += dt;
    state_.controller.time_since_rotation_input.z += dt;

    if (speed_sampling_enabled) {
        --speed_sample_ticks_remaining;
        if (speed_sample_ticks_remaining <= 0) {
            sample_speed();
            speed_sample_ticks_remaining = speed_sample_tick_period;
        }
    }
}

void Sim::think(float const dt) {
    planned_state_ = state_;
    auto& state{planned_state_};
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::think");

    update_boost_brake(dt, state);
    update_body_orientation(dt, state);
    integrate_flight_model(dt, get_active_flight_model_config(), flight_intent_, state);
}

void Sim::apply_movement() {
    SANDBOX_PROFILE_SCOPE("PlayerShipSim::apply_movement");
    state_ = planned_state_;
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
    auto const original_health{health_table_.get_health(health_index_, unique_entity_id)};
    auto& health{health_ref()};
    EntityUniqueId killer{};
    auto const damage_count{damage_events.num()};
    for (std::int32_t event_index{}; event_index < damage_count; ++event_index) {
        auto const element{static_cast<std::size_t>(event_index)};
        assert(damage_events.damaged_entities[element] == unique_entity_id);
        if (is_dead(health)) {
            continue;
        }

        auto const requested_damage{damage_events.damage_amounts[element]};
        assert(requested_damage >= 0);
        if (requested_damage == 0) {
            continue;
        }
        auto const was_alive{sim::is_alive(health)};
        auto const applied_damage{std::min(health, requested_damage)};
        health -= requested_damage;
        ledger_.record_damage(unique_entity_id, damage_events.instigators[element], applied_damage);
        if (was_alive && is_dead(health)) {
            killer = damage_events.instigators[element];
        }
    }

    if (sim::is_alive(original_health) && is_dead(health)) {
        die(killer);
    }
}

/* **************************************** */
// Identity and accounting
/* **************************************** */
void Sim::set_team(Team const new_team) noexcept {
    team = new_team;
    if (unique_entity_id.is_valid()) {
        ledger_.record_status(unique_entity_id, team, is_alive());
    }
}

/* **************************************** */
// Movement
/* **************************************** */
void Sim::update_body_orientation(float const dt, PlayerSimulationState& state) {
    auto& presentation{state.presentation};
    auto const current_rotation{presentation.body_transform.rotator()};
    auto const target_pitch{flight_intent_.rotation.x * config.pitch_angle_max};
    auto const new_pitch{player_movement::interpolate_to(
        current_rotation.pitch, target_pitch, dt, config.pitch_speed)};

    auto const target_yaw{flight_intent_.rotation.y * config.yaw_angle_max};
    auto const new_yaw{
        player_movement::interpolate_to(current_rotation.yaw, target_yaw, dt, config.yaw_speed)};

    auto const turn_target{flight_intent_.rotation.y * config.turn_bank_angle_max};
    auto const turn_speed{flight_intent_.rotation.y * config.turn_bank_speed};
    auto const roll_speed{
        std::max(static_cast<double>(config.turn_bank_speed), std::abs(turn_speed))};
    auto const new_roll{
        player_movement::interpolate_to(current_rotation.roll, turn_target, dt, roll_speed)};
    presentation.body_transform.rotation = to_quaternion(Rotator3d{new_pitch, new_yaw, new_roll});
}

void Sim::update_boost_brake(float const dt, PlayerSimulationState& state) {
    auto& controller{state.controller};
    auto& resources{state.resources};
    auto const& model{get_active_flight_model_config()};
    refresh_effective_action(state);

    auto energy_change{model.energy_recharge_per_second};
    switch (controller.effective_action) {
        case BoostBrakeState::None:
            break;
        case BoostBrakeState::Brake:
            energy_change = -model.brake.energy_drain_per_second;
            break;
        case BoostBrakeState::Boost:
            energy_change = -model.boost.energy_drain_per_second;
            break;
        case BoostBrakeState::EmergencyBrake:
            energy_change = -model.emergency_brake.energy_drain_per_second;
            break;
    }
    resources.thrust_change_rate = energy_change * config.thrust_energy_max;
    resources.thrust_energy += dt * resources.thrust_change_rate;
    resources.thrust_energy = std::clamp(resources.thrust_energy, 0.f, config.thrust_energy_max);
    if (resources.thrust_energy <= 0.f) {
        refresh_effective_action(state);
    }
}

void Sim::refresh_effective_action(PlayerSimulationState& state) noexcept {
    auto const& model{get_active_flight_model_config()};
    auto const has_energy = [&state](float const drain) {
        return drain == 0.f || state.resources.thrust_energy > 0.f;
    };
    auto action{BoostBrakeState::None};
    if (flight_intent_.emergency_brake_held && model.emergency_brake.available &&
        has_energy(model.emergency_brake.energy_drain_per_second)) {
        action = BoostBrakeState::EmergencyBrake;
    } else if (flight_intent_.brake_held && model.brake.available &&
               has_energy(model.brake.energy_drain_per_second)) {
        action = BoostBrakeState::Brake;
    } else if ((flight_intent_.boost_held ||
                (model.boost.accelerator_activates_boost && flight_intent_.accelerator > 0.f)) &&
               model.boost.available && has_energy(model.boost.energy_drain_per_second)) {
        action = BoostBrakeState::Boost;
    }

    if (action == BoostBrakeState::Boost &&
        state.controller.effective_action != BoostBrakeState::Boost) {
        ++state.presentation.boost_start_sequence;
    }
    if (action != state.controller.effective_action) {
        prepare_flight_model_action_transition(state, model);
    }
    state.controller.effective_action = action;
    clamp_flight_model_persistent_targets(state, model);
}

/* **************************************** */
// Flight controls
/* **************************************** */
void Sim::set_forward_input(float const input) noexcept {
    flight_intent_.translation.x = std::clamp(input, -1.f, 1.f);
}

void Sim::set_right_input(float const input) noexcept {
    flight_intent_.translation.y = std::clamp(input, -1.f, 1.f);
}

void Sim::set_up_input(float const input) noexcept {
    flight_intent_.translation.z = std::clamp(input, -1.f, 1.f);
}

void Sim::set_ship_2d_control(ml::Vector2d const input) {
    if (!sampling_target_speed_) {
        return;
    }
    sampled_target_speed_scale_ = {
        std::clamp(input.x, -1.0, 1.0),
        std::clamp(input.y, -1.0, 1.0),
    };
}

void Sim::set_accelerator(float const input) noexcept {
    flight_intent_.accelerator = std::clamp(input, 0.f, 1.f);
}

void Sim::start_sampling() noexcept {
    if (sampling_target_speed_) {
        return;
    }
    sampled_target_speed_scale_ = {};
    sampling_target_speed_ = true;
}

void Sim::stop_sampling() {
    if (!sampling_target_speed_) {
        return;
    }
    sampling_target_speed_ = false;
    auto const& config{get_active_flight_model_config()};
    auto const boosting{state_.controller.effective_action == BoostBrakeState::Boost};
    auto const& forward{boosting ? config.translation.forward.boosted
                                 : config.translation.forward.normal};
    auto const& right{boosting ? config.translation.right.boosted
                               : config.translation.right.normal};
    state_.controller.persistent_forward_target_speed = player_movement::target_speed_from_input(
        forward, static_cast<float>(sampled_target_speed_scale_.y));
    state_.controller.persistent_right_target_speed = player_movement::target_speed_from_input(
        right, static_cast<float>(sampled_target_speed_scale_.x));
    planned_state_ = state_;
}

void Sim::adjust_desired_forward_velocity(float const direction) {
    if (sampling_target_speed_ || std::abs(direction) <= 1.e-8f) {
        return;
    }
    auto const& axis{get_active_flight_model_config().translation.forward};
    auto const& drive{state_.controller.effective_action == BoostBrakeState::Boost ? axis.boosted
                                                                                   : axis.normal};
    auto const adjustment{
        std::copysign(player_movement::target_speed_bound(drive, direction) * 0.05f, direction)};
    state_.controller.persistent_forward_target_speed =
        std::clamp(state_.controller.persistent_forward_target_speed + adjustment,
                   -player_movement::target_speed_bound(drive, -1.f),
                   player_movement::target_speed_bound(drive, 1.f));
    planned_state_ = state_;
}

void Sim::set_pitch_input(float const input) noexcept {
    flight_intent_.rotation.x = std::clamp(input, -1.f, 1.f);
}

void Sim::set_yaw_input(float const input) noexcept {
    flight_intent_.rotation.y = std::clamp(input, -1.f, 1.f);
}

void Sim::set_roll_input(float const input) noexcept {
    flight_intent_.rotation.z = std::clamp(input, -1.f, 1.f);
}

void Sim::start_boost() {
    flight_intent_.boost_held = true;
    refresh_effective_action(state_);
    planned_state_ = state_;
}

void Sim::stop_boost() {
    flight_intent_.boost_held = false;
    refresh_effective_action(state_);
    planned_state_ = state_;
}

void Sim::start_brake() {
    flight_intent_.brake_held = true;
    refresh_effective_action(state_);
    planned_state_ = state_;
}

void Sim::start_emergency_brake() {
    flight_intent_.emergency_brake_held = true;
    refresh_effective_action(state_);
    planned_state_ = state_;
}

void Sim::stop_emergency_brake() {
    flight_intent_.emergency_brake_held = false;
    refresh_effective_action(state_);
    planned_state_ = state_;
}

void Sim::stop_brake() {
    flight_intent_.brake_held = false;
    refresh_effective_action(state_);
    planned_state_ = state_;
}

void Sim::select_flight_model_slot(FlightModelSlot const slot) noexcept {
    if (active_flight_model_slot_ == slot) {
        return;
    }

    active_flight_model_slot_ = slot;
    sampling_target_speed_ = false;
    sampled_target_speed_scale_ = {};
    reset_flight_model_controller(state_, get_active_flight_model_config());
    refresh_effective_action(state_);
    planned_state_ = state_;
}

auto Sim::set_flight_model_slot_profile(FlightModelSlot const slot,
                                        FlightModelProfile profile) noexcept -> bool {
    if (!validate_flight_model_config(profile.config)) {
        return false;
    }

    flight_model_profile(flight_models_, slot) = std::move(profile);
    if (active_flight_model_slot_ == slot) {
        reset_flight_model_controller(state_, get_active_flight_model_config(), false);
        refresh_effective_action(state_);
        planned_state_ = state_;
    }
    return true;
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
            auto const middle{(middle_socket * planned_state_.presentation.body_transform *
                               planned_state_.physical.transform)};
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
                (middle_socket * state_.presentation.body_transform * state_.physical.transform)};
            fire_lasers_from(fire_points);
            break;
        }
        case ShipLaserMode::Double:
        case ShipLaserMode::Hyper: {
            std::array<Transform3d, 2> const fire_points{
                left_socket * state_.presentation.body_transform * state_.physical.transform,
                right_socket * state_.presentation.body_transform * state_.physical.transform};
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
    new_lasers.add_uninitialised(laser_count);
    auto const laser_columns{new_lasers.get_view()};

    for (std::int32_t i{0}; i < laser_count; ++i) {
        set_vector(laser_columns.view_locations(), i, to_float(fire_points[i].location));
        set_rotation(laser_columns.view_rotations(), i, to_float(fire_points[i].rotator()));
        set_vector(laser_columns.view_base_velocities(), i, to_float(state_.physical.velocity));
    }

    std::ranges::fill(laser_columns.damages(), config.laser.damage);
    std::ranges::fill(laser_columns.speeds(), config.laser.projectile_speed);
    std::ranges::fill(laser_columns.max_distances(), config.laser.max_distance);
    std::ranges::fill(laser_columns.sources(), LaserSource{team, EntityType::PlayerShip});
    std::ranges::fill(laser_columns.instigator_ids(), unique_entity_id);
    lasers.queue_laser_spawns(new_lasers.get_const_view());
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
    if (!is_alive()) {
        return;
    }
    set_health(health_table_.get_health(health_index_, unique_entity_id) + added_health);
}

void Sim::set_health(Health const new_health, EntityUniqueId const killer) {
    auto& health{health_ref()};
    if (new_health == health || !sim::is_alive(health)) {
        return;
    }

    auto const was_alive{sim::is_alive(health)};
    health = std::min(new_health, max_health_);

    if (was_alive && !sim::is_alive(health)) {
        die(killer);
    }
}

auto Sim::get_health() const -> ShipHealth {
    return {health_table_.get_health(health_index_, unique_entity_id), max_health_};
}

auto Sim::is_alive() const -> bool {
    return health_index_.is_valid() &&
           sim::is_alive(health_table_.get_health(health_index_, unique_entity_id));
}

auto Sim::health_ref() -> Health& {
    return health_table_
        .get_view(std::span<HealthIndex const>{&health_index_, 1},
                  std::span<EntityUniqueId const>{&unique_entity_id, 1})
        .health(0);
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
    return static_cast<float>(state_.physical.velocity.size());
}

auto Sim::energy_is_full() const -> bool {
    return state_.resources.thrust_energy == config.thrust_energy_max;
}

auto Sim::get_energy() const -> float {
    assert(config.thrust_energy_max > 0.f);
    return state_.resources.thrust_energy / config.thrust_energy_max;
}

auto Sim::get_middle_socket() const -> Transform3d {
    return middle_socket * state_.presentation.body_transform * state_.physical.transform;
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
        std::clamp(state_.physical.velocity.size(), 0.0, 100e3)};
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
