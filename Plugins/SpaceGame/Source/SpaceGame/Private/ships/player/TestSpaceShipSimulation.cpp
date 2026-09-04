#include "SpaceGame/ships/player/TestSpaceShipSimulation.h"

#include <SandboxGameShared/utilities/enums.h>
#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/entities/EntityDeathInfo.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestTeamVisualData.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <limits>
#include <utility>

namespace ml::test_space_ship {
void Simulation::set_config(FPlayerShipConfig const& new_config) noexcept {
    config = &new_config;
}

void Simulation::set_entity_registry(FTestEntityRegistry& new_entity_registry) noexcept {
    entity_registry = &new_entity_registry;
}

void Simulation::set_spatial_query_manager(FSpatialQueryManager const& new_query_manager) noexcept {
    spatial_query_manager = &new_query_manager;
}

void Simulation::set_lasers(ml::test_lasers::Simulation& new_lasers) noexcept {
    lasers = &new_lasers;
}

void Simulation::bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) {
    simulation_clock.bind(orchestrator);
}

void Simulation::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::begin_play);
    check(config);
    check(entity_registry);
    check(spatial_query_manager);
    check(lasers);
    check(IsValid(config->team_visual_data));
    check(simulation_clock.is_valid());

    velocity = FVector::ZeroVector;
    thrust_energy = config->thrust_energy_max;
    set_laser_mode(ELaserFiringState::idle);
    set_laser_fire_rate(laser_fire_rate);
    configure_speed_sampling();
    set_boost_brake_state(EBoostBrakeState::None);
    register_with_entity_registry();
    health.clamp_to_max();
}

void Simulation::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::begin_tick);
}

void Simulation::update_timers(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::update_timers);

    laser_shot_cooldown -= dt;
    time_since_rotation_input += dt;

#if WITH_EDITOR
    --speed_sample_ticks_remaining;
    if (speed_sample_ticks_remaining <= 0) {
        sample_speed();
        speed_sample_ticks_remaining = speed_sample_tick_period;
    }
#endif
}

void Simulation::move(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::move);

    update_boost_brake(dt);
    update_rotation(dt);
    update_visual_orientation(dt);
    integrate_velocity(dt);

    auto const lateral_speed{planar_movement_direction.X * config->lateral_adjustment_speed};
    auto const vertical_speed{planar_movement_direction.Y * config->vertical_adjustment_speed};
    auto const local_adjustment{FVector{0.f, lateral_speed, vertical_speed}};
    velocity += transform.TransformVectorNoScale(local_adjustment);
    transform.AddToTranslation(velocity * dt);
}

void Simulation::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::queue_commands);
    update_laser_firing();
}

void Simulation::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::resolve_damage_events);

    auto const original_health{health.health};
    FRegistryEntityHandle killer{};
    auto const& direct_damage{entity_registry->get_direct_damage_queue_view()};
    auto const damage_count{direct_damage.num()};
    for (int32 i{0}; i < damage_count; ++i) {
        if (direct_damage.damaged_entities[i] != registry_handle) {
            continue;
        }

        auto const was_alive{health.is_alive()};
        health.health -= direct_damage.damage_amounts[i];
        if (was_alive && !health.is_alive()) {
            killer = direct_damage.instigators[i];
        }
    }

    if (original_health > 0 && !health.is_alive()) {
        die(killer);
    }
}

void Simulation::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::update_entity_registry);
    queue_entity_update(EntityDeathInfo{});
}

void Simulation::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::sync_from_registry);
}

void Simulation::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::PlayerShipSimulation::end_tick);
}

void Simulation::register_with_entity_registry() {
    auto const new_entities{
        entity_registry->add_entities(get_entity_update_data().get_const_view())};
    registry_handle = new_entities.registry_handles[0];
    unique_entity_id = new_entities.first_id;
    check(entity_registry->is_valid_unique_id(unique_entity_id));

    update_entity_registry();
}

auto Simulation::get_entity_update_data() const -> RegistryEntityData {
    RegistryEntityData entity_data;
    ml::append(entity_data.locations, transform.GetLocation());
    entity_data.velocities.add(FVector3f{velocity});
    entity_data.radii.Add(collision_radius);
    entity_data.healths.Add(health.health);
    entity_data.teams.Add(team);
    entity_data.alive.Add(static_cast<uint8>(health.is_alive()));
    entity_data.entity_types.Add(ETestEntityType::PlayerShip);
    return entity_data;
}

void Simulation::queue_entity_update(EntityDeathInfo const& death_info) {
    entity_registry->queue_entity_updates(
        FTestEntityRegistry::ConstView{
            {&registry_handle, 1},
            get_entity_update_data().get_const_view(),
        },
        death_info);
}

void Simulation::integrate_velocity(float const dt) {
    switch (flight_mode) {
        case ETestSpaceShipFlightMode::ForwardSpeed: {
            auto const new_speed{forward_flight_model.update(dt)};
            velocity = transform.GetUnitAxis(EAxis::X) * new_speed;
            break;
        }
        case ETestSpaceShipFlightMode::PlanarVelocity: {
            planar_velocity = planar_flight_model.update(dt);
            velocity = planar_velocity;
            break;
        }
    }
}

void Simulation::update_rotation(float const dt) {
    auto const rotation_step{config->rotation_speed * dt};
    if (rotation_input != FVector2D::ZeroVector || !FMath::IsNearlyZero(roll_input)) {
        auto const yaw_strength{FMath::Abs(rotation_input.X)};
        auto const yaw_step{config->rotation_speed * yaw_strength * dt};
        auto const delta_rotation{FRotator{rotation_input.Y * rotation_step,
                                           rotation_input.X * yaw_step,
                                           roll_input * rotation_step}};
        transform.ConcatenateRotation(delta_rotation.Quaternion());
        transform.NormalizeRotation();
        time_since_rotation_input = 0.f;
        return;
    }

    if (time_since_rotation_input >= config->auto_level_roll_delay) {
        auto const rotation{transform.Rotator()};
        auto const roll{FMath::FInterpTo(rotation.Roll, 0.f, dt, config->auto_level_speed)};
        transform.SetRotation(FRotator{rotation.Pitch, rotation.Yaw, roll}.Quaternion());
    }
}

void Simulation::update_visual_orientation(float const dt) {
    auto const current_rotation{visual_transform.Rotator()};
    auto const target_pitch{rotation_input.Y * config->pitch_angle_max};
    auto const new_pitch{
        FMath::FInterpTo(current_rotation.Pitch, target_pitch, dt, config->pitch_speed)};
    auto const target_yaw{rotation_input.X * config->yaw_angle_max};
    auto const new_yaw{FMath::FInterpTo(current_rotation.Yaw, target_yaw, dt, config->yaw_speed)};
    auto const turn_target{rotation_input.X * config->turn_bank_angle_max};
    auto const turn_speed{rotation_input.X * config->turn_bank_speed};
    auto const roll_speed{FMath::Max(config->turn_bank_speed, FMath::Abs(turn_speed))};
    auto const new_roll{FMath::FInterpTo(current_rotation.Roll, turn_target, dt, roll_speed)};
    visual_transform.SetRotation(FRotator{new_pitch, new_yaw, new_roll}.Quaternion());
}

void Simulation::set_boost_brake_state(EBoostBrakeState const state) {
    auto const current_speed{get_speed()};
    auto const& speed_responses{config->speed_responses};
    FSpeedResponse response{speed_responses.accelerating_to_cruise};

    switch (state) {
        case EBoostBrakeState::Boost: {
            target_speed = config->boost_speed;
            thrust_change_rate = -(1.f / config->boost_depletion_time);
            response = speed_responses.boost;
            break;
        }
        case EBoostBrakeState::Brake: {
            target_speed = config->brake_speed;
            thrust_change_rate = -(1.f / config->brake_depletion_time);
            response = speed_responses.brake;
            break;
        }
        default: {
            UE_LOG(LogSandbox, Error, TEXT("Unhandled player boost/brake state."));
            [[fallthrough]];
        }
        case EBoostBrakeState::None: {
            target_speed = config->cruise_speed;
            thrust_change_rate = 1.f / config->thrust_recharge_time;
            if (target_speed < current_speed) {
                response = speed_responses.slowing_to_cruise;
            }
            break;
        }
    }

    forward_flight_model.set_new_impulse(response, current_speed, target_speed);
    boost_brake_state = state;
}

void Simulation::update_boost_brake(float const dt) {
    auto const starting_energy{thrust_energy};
    if (starting_energy <= 0.f) {
        set_boost_brake_state(EBoostBrakeState::None);
    }

    thrust_energy += dt * thrust_change_rate;
    thrust_energy = FMath::Clamp(thrust_energy, 0.f, config->thrust_energy_max);
}

void Simulation::set_move_input(FVector2D const input) noexcept {
    planar_movement_direction = input;
}

void Simulation::set_lateral_move_input(float const input) noexcept {
    planar_movement_direction.X = input;
}

void Simulation::set_vertical_move_input(float const input) noexcept {
    planar_movement_direction.Y = input;
}

void Simulation::set_ship_2d_control(FVector2D const input) {
    if (!sampling) {
        return;
    }

    if (control_mode == ETestSpaceShipControlMode::Velocity) {
        target_local_planar_velocity_scale = input;
    }
}

void Simulation::set_ship_1d_control_x(float const input) {
    auto control{target_local_planar_velocity_scale};
    control.X = input;
    set_ship_2d_control(control);
}

void Simulation::set_ship_1d_control_y(float const input) {
    auto control{target_local_planar_velocity_scale};
    control.Y = input;
    set_ship_2d_control(control);
}

void Simulation::select_next_control_mode() {
    control_mode = ml::get_next(control_mode);
}

void Simulation::select_previous_control_mode() {
    control_mode = ml::get_previous(control_mode);
}

void Simulation::start_sampling() noexcept {
    sampling = true;
}

void Simulation::stop_sampling() {
    sampling = false;
    if (control_mode != ETestSpaceShipControlMode::Velocity) {
        return;
    }

    auto const world_direction{
        transform.GetUnitAxis(EAxis::X) * target_local_planar_velocity_scale.Y +
        transform.GetUnitAxis(EAxis::Y) * target_local_planar_velocity_scale.X};
    target_local_planar_velocity = world_direction * config->cruise_speed;
    planar_flight_model.set_new_impulse(config->speed_responses.accelerating_to_cruise,
                                        planar_velocity,
                                        target_local_planar_velocity);
}

void Simulation::turn(FVector2D const direction) noexcept {
    rotation_input = direction;
}

void Simulation::start_boost() {
    if (energy_is_full() && boost_brake_state == EBoostBrakeState::None) {
        set_boost_brake_state(EBoostBrakeState::Boost);
    }
}

void Simulation::stop_boost() {
    if (boost_brake_state == EBoostBrakeState::Boost) {
        set_boost_brake_state(EBoostBrakeState::None);
    }
}

void Simulation::start_brake() {
    if (energy_is_full() && boost_brake_state == EBoostBrakeState::None) {
        set_boost_brake_state(EBoostBrakeState::Brake);
    }
}

void Simulation::stop_brake() {
    if (boost_brake_state == EBoostBrakeState::Brake) {
        set_boost_brake_state(EBoostBrakeState::None);
    }
}

void Simulation::roll(float const direction) noexcept {
    roll_input = FMath::Clamp(direction, -1.f, 1.f);
}

void Simulation::set_flight_mode(ETestSpaceShipFlightMode const new_flight_mode) noexcept {
    flight_mode = new_flight_mode;
}

void Simulation::set_lock_on_target(FRegistryEntityHandle const target) noexcept {
    lock_on_target = target;
}

void Simulation::set_laser_mode(ELaserFiringState const mode) noexcept {
    laser_firing_mode = mode;
}

void Simulation::update_laser_firing() {
    auto const cooldown_finished{laser_shot_cooldown <= 0.f};
    switch (laser_firing_mode) {
        case ELaserFiringState::idle: {
            break;
        }
        case ELaserFiringState::burst: {
            if (cooldown_finished) {
                fire_laser();
                laser_shot_cooldown = config->laser.fire_cooldown;
                if (lasers_fired_this_burst >= lasers_per_burst) {
                    laser_shot_cooldown = config->laser_lock_on_transition_delay;
                    set_laser_mode(ELaserFiringState::lock_on_transition);
                }
            }
            break;
        }
        case ELaserFiringState::lock_on_transition: {
            if (cooldown_finished) {
                set_laser_mode(ELaserFiringState::lock_on_searching);
            }
            [[fallthrough]];
        }
        case ELaserFiringState::lock_on_searching: {
            auto const middle{get_middle_socket()};
            auto const start{middle.GetLocation()};
            auto const end{start + middle.GetUnitAxis(EAxis::X) * config->laser_lock_on_distance};
            auto const hit{spatial_query_manager->trace_closest(
                FVector3f{start}, FVector3f{end}, registry_handle)};
            if (hit.hit && hit.entity.is_valid()) {
                set_lock_on_target(hit.entity);
                set_laser_mode(ELaserFiringState::lock_on_acquired);
            }
            break;
        }
        case ELaserFiringState::lock_on_acquired: {
            break;
        }
    }
}

void Simulation::start_fire_laser() {
    set_laser_mode(ELaserFiringState::burst);
    lasers_fired_this_burst = 0;
    laser_shot_cooldown = 0.f;
    set_lock_on_target({});
}

void Simulation::stop_fire_laser() {
    if (laser_firing_mode == ELaserFiringState::lock_on_acquired) {
        set_lock_on_target({});
    }
    set_laser_mode(ELaserFiringState::idle);
}

void Simulation::fire_laser() {
    switch (laser_mode) {
        case EShipLaserMode::Single: {
            TStaticArray<FTransform, 1> const fire_points{get_middle_socket()};
            fire_lasers_from(fire_points);
            break;
        }
        case EShipLaserMode::Double:
        case EShipLaserMode::Hyper: {
            TStaticArray<FTransform, 2> const fire_points{
                left_socket * visual_transform * transform,
                right_socket * visual_transform * transform};
            fire_lasers_from(fire_points);
            break;
        }
        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("Unhandled player laser mode."));
            break;
        }
    }

    ++lasers_fired_this_burst;
    laser_shot_cooldown = config->laser.fire_cooldown;
}

void Simulation::fire_lasers_from(TConstArrayView<FTransform> const fire_points) {
    ml::test_lasers::SpawnRequests new_lasers;
    auto const colour_cache{UTestTeamVisualData::build_team_colour_cache(config->team_visual_data)};
    auto const laser_count{fire_points.Num()};
    ml::add_uninitialised(laser_count, new_lasers);

    for (int32 i{0}; i < laser_count; ++i) {
        ml::assign(new_lasers.locations, i, fire_points[i].GetLocation());
        ml::assign(new_lasers.rotations, i, fire_points[i].Rotator());
        ml::assign(new_lasers.base_velocities, i, FVector3f{velocity});
    }

    new_lasers.set_damages(config->laser.damage);
    new_lasers.set_speeds(config->laser.projectile_speed);
    new_lasers.set_max_distances(config->laser.max_distance);
    new_lasers.set_colours(colour_cache[team]);
    ml::fill(new_lasers.instigator_handles, registry_handle);
    lasers->queue_laser_spawns(new_lasers);
}

void Simulation::upgrade_laser() noexcept {
    if (laser_mode == EShipLaserMode::Single) {
        laser_mode = EShipLaserMode::Double;
    } else if (laser_mode == EShipLaserMode::Double) {
        laser_mode = EShipLaserMode::Hyper;
    }
}

void Simulation::select_next_laser_fire_rate() noexcept {
    set_laser_fire_rate(ml::get_next(laser_fire_rate));
}

void Simulation::select_previous_laser_fire_rate() noexcept {
    set_laser_fire_rate(ml::get_previous(laser_fire_rate));
}

void Simulation::set_laser_fire_rate(ETestShipFireRate const value) noexcept {
    laser_fire_rate = value;
    switch (laser_fire_rate) {
        case ETestShipFireRate::Single: {
            lasers_per_burst = 1;
            break;
        }
        case ETestShipFireRate::Burst3: {
            lasers_per_burst = 3;
            break;
        }
        case ETestShipFireRate::FullAuto: {
            lasers_per_burst = std::numeric_limits<decltype(lasers_per_burst)>::max();
            break;
        }
    }
}

void Simulation::add_health(int32 const added_health) {
    set_health(health.health + added_health);
}

void Simulation::set_health(int32 const new_health, FRegistryEntityHandle const killer) {
    if (new_health == health.health) {
        return;
    }

    auto const was_alive{health.is_alive()};
    health.health = FMath::Min(new_health, health.max_health);
    if (was_alive && !health.is_alive()) {
        die(killer);
    }
}

void Simulation::die(FRegistryEntityHandle const killer) {
    EntityDeathInfo death_info;
    auto const reason{killer.is_null() ? ETestDeathReason::Unknown : ETestDeathReason::Combat};
    death_info.add(reason, registry_handle, killer);
    queue_entity_update(death_info);
    death_notification_pending = true;
}

auto Simulation::consume_death_notification() noexcept -> bool {
    return std::exchange(death_notification_pending, false);
}

auto Simulation::get_kills() const -> int32 {
    return entity_registry->get_kills(unique_entity_id);
}

auto Simulation::get_speed() const noexcept -> float {
    return velocity.Size();
}

auto Simulation::energy_is_full() const -> bool {
    check(config);
    return thrust_energy == config->thrust_energy_max;
}

auto Simulation::get_energy() const -> float {
    check(config);
    check(config->thrust_energy_max > 0.f);
    return thrust_energy / config->thrust_energy_max;
}

auto Simulation::get_middle_socket() const -> FTransform {
    return middle_socket * visual_transform * transform;
}

#if WITH_EDITOR
void Simulation::sample_speed() {
    speed_samples[speed_sample_index] = {
        FMath::Clamp(simulation_clock.get_simulation_time(), 0.0, 1e9),
        FMath::Clamp(velocity.Size(), 0.0, 100e3)};
    ++speed_sample_index;
    if (speed_sample_index >= speed_sample_max) {
        speed_sample_index = 0;
    }
}
#endif

void Simulation::configure_speed_sampling() {
#if WITH_EDITOR
    static constexpr double sample_rate_hz{60.0};
    static constexpr double sample_window_seconds{5.0};
    auto const sample_tick_period{simulation_clock.frequency_to_tick_period(sample_rate_hz)};
    auto const sample_window_ticks{simulation_clock.duration_to_tick_period(sample_window_seconds)};
    check(std::in_range<int32>(sample_tick_period));
    check(sample_tick_period > 0);
    auto const sample_count{(sample_window_ticks + sample_tick_period - 1) / sample_tick_period};
    check(std::in_range<int32>(sample_count));

    speed_sample_index = 0;
    speed_sample_max = static_cast<int32>(sample_count);
    speed_sample_tick_period = static_cast<int32>(sample_tick_period);
    speed_sample_ticks_remaining = speed_sample_tick_period;
    speed_samples.Init(FVector2d::ZeroVector, speed_sample_max);
#endif
}
} // namespace ml::test_space_ship
