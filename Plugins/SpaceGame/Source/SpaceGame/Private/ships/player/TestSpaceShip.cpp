#include "SpaceGame/ships/player/TestSpaceShip.h"

#include <SandboxGameShared/utilities/enums.h>
#include <SpaceGame/combat/lasers/TestLasers.h>
#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/entities/EntityDeathInfo.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityRegistryData.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/entities/TestTeamVisualData.h>
#include <SpaceGame/ships/common/ShipHealthComponent.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/mesh.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Camera/CameraComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/HitResult.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <limits>
#include <NiagaraComponent.h>
#include <utility>

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

ATestSpaceShip::ATestSpaceShip()
    : camera(CreateDefaultSubobject<UCameraComponent>(TEXT("camera")))
    , ship_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ship_mesh")))
    , boost_pulse{CreateDefaultSubobject<UNiagaraComponent>(TEXT("boost_effect"))}
    , boost_engine_effect{CreateDefaultSubobject<UNiagaraComponent>(TEXT("boost_engine_effect"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    camera->SetupAttachment(RootComponent);
    ship_mesh->SetupAttachment(RootComponent);

    boost_pulse->SetupAttachment(RootComponent);
    boost_pulse->bAutoActivate = false;
    boost_pulse->SetAutoDestroy(false);

    boost_engine_effect->SetupAttachment(ship_mesh);
    boost_engine_effect->bAutoActivate = false;
    boost_engine_effect->SetAutoDestroy(false);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    configure_ship_mesh();
}

auto ATestSpaceShip::make_simulation() const -> ml::test_space_ship::Simulation {
    return {
        .team = team,
        .flight_mode = flight_mode,
        .control_mode = control_mode,
        .laser_mode = laser_mode,
        .laser_fire_rate = laser_fire_rate,
        .health = health,
    };
}

void ATestSpaceShip::bind_simulation(ml::test_space_ship::Simulation& new_simulation) {
    if (standalone_simulation.IsSet()) {
        new_simulation = MoveTemp(standalone_simulation.GetValue());
        standalone_simulation.Reset();
    } else {
        new_simulation = make_simulation();
    }
    bound_simulation = &new_simulation;
}

void ATestSpaceShip::unbind_simulation() {
    bound_simulation = nullptr;
}

auto ATestSpaceShip::simulation() -> ml::test_space_ship::Simulation& {
    if (bound_simulation) {
        return *bound_simulation;
    }
    if (!standalone_simulation.IsSet()) {
        standalone_simulation.Emplace(make_simulation());
    }
    return standalone_simulation.GetValue();
}

auto ATestSpaceShip::simulation() const -> ml::test_space_ship::Simulation const& {
    return const_cast<ATestSpaceShip*>(this)->simulation();
}

void ATestSpaceShip::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::begin_play);
    check(entity_registry);

    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("ATestSpaceShip actor_config is nullptr."));
    }

    ml::fatal_if_uobject_ptrs_invalid({
        {
            SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
            SANDBOX_NAMED_UOBJECT_PTR(ship_mesh),
            SANDBOX_NAMED_UOBJECT_PTR(boost_pulse),
        },
        {
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data),
        },
    });

    auto& state{simulation()};
    state.velocity = GetActorForwardVector() * actor_config->cruise_speed;
    state.thrust_energy = actor_config->thrust_energy_max;

    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::left));
    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::right));
    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::middle));

    if (!simulation_clock.is_valid()) {
        UE_LOG(LogSandbox, Fatal, TEXT("Simulation clock is invalid"));
    }

    set_laser_mode(ELaserFiringState::idle);
    set_laser_fire_rate(state.laser_fire_rate);

    configure_speed_sampling();

    state.velocity = FVector3d::ZeroVector;
    set(EBoostBrakeState::None);

    configure_boost_pulse();
    configure_boost_engine_effect();

    register_with_entity_registry();

    state.health.clamp_to_max();
}
void ATestSpaceShip::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::begin_tick);
}
void ATestSpaceShip::update_timers(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::update_timers);

    log_config.tick(dt);

    auto& state{simulation()};
    state.laser_shot_cooldown -= dt;
    state.time_since_rotation_input += dt;

#if WITH_EDITOR
    --state.speed_sample_ticks_remaining;
    if (state.speed_sample_ticks_remaining <= 0) {
        sample_speed();
        state.speed_sample_ticks_remaining = state.speed_sample_tick_period;
    }
#endif
}
void ATestSpaceShip::move(float const dt) {
    auto& state{simulation()};
    update_boost_brake(dt);
    update_actor_rotation(dt);
    update_visual_orientation(dt);
    integrate_velocity(dt);

    auto const lateral_adjustment_speed{state.planar_movement_direction.X *
                                        actor_config->lateral_adjustment_speed};
    auto const vertical_adjustment_speed{state.planar_movement_direction.Y *
                                         actor_config->vertical_adjustment_speed};
    auto const adjustment_velocity{
        FVector{0.f, lateral_adjustment_speed, vertical_adjustment_speed}};
    auto const world_planar_velocity{
        GetActorTransform().TransformVectorNoScale(adjustment_velocity)};

    state.velocity += world_planar_velocity;

    auto const translation{state.velocity * dt};
    SetActorLocation(GetActorLocation() + translation, sweep_movement);
}
void ATestSpaceShip::queue_commands() {
    update_laser_firing();
}
void ATestSpaceShip::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::resolve_damage_events);

    auto& state{simulation()};
    auto const original_health{state.health.health};
    FRegistryEntityHandle killer{};

    auto const apply_damage{
        [this, &killer](int32 const damage, FRegistryEntityHandle const instigator) {
            auto const was_alive{is_alive()};
            simulation().health.health -= damage;
            if (was_alive && !is_alive()) {
                killer = instigator;
            }
        }};

    auto const& direct_damage{entity_registry->get_direct_damage_queue_view()};
    auto const n_direct_damage{direct_damage.num()};
    for (int32 i{0}; i < n_direct_damage; ++i) {
        if (direct_damage.damaged_entities[i] != state.registry_handle) {
            continue;
        }

        apply_damage(direct_damage.damage_amounts[i], direct_damage.instigators[i]);
    }

    if (state.health.health != original_health) {
        on_health_changed.ExecuteIfBound(state.health);
    }
    if ((original_health > 0) && !is_alive()) {
        die(killer);
    }
}

auto ATestSpaceShip::get_collision_mesh() const -> UStaticMesh const* {
    return ship_mesh ? ship_mesh->GetStaticMesh() : nullptr;
}

void ATestSpaceShip::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::update_entity_registry);

    queue_entity_update(EntityDeathInfo{});
}
void ATestSpaceShip::queue_entity_update(EntityDeathInfo const& death_info) {
    entity_registry->queue_entity_updates(
        FTestEntityRegistry::ConstView{
            {&simulation().registry_handle, 1},
            get_entity_update_data().get_const_view(),
        },
        death_info);
}
void ATestSpaceShip::resolve_damage_targets() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::resolve_damage_targets);
}
void ATestSpaceShip::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::sync_from_registry);
}
void ATestSpaceShip::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::update_visual_data);
    boost_engine_effect->SetVectorParameter(TEXT("ship_velocity"), simulation().velocity);
}
void ATestSpaceShip::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::commit_visual_data);
    draw_debug_shapes();
}
void ATestSpaceShip::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::end_tick);
    log_config.on_tick_end();
}

/* ---------------------------------------------------------------------------------------------- */
// Entity data
/* ---------------------------------------------------------------------------------------------- */
void ATestSpaceShip::register_with_entity_registry() {
    auto const new_entities{
        entity_registry->add_entities(get_entity_update_data().get_const_view())};
    auto& state{simulation()};
    state.registry_handle = new_entities.registry_handles[0];
    state.unique_entity_id = new_entities.first_id;

    check(entity_registry->is_valid_unique_id(state.unique_entity_id));

    update_entity_registry();
}
auto ATestSpaceShip::get_entity_update_data() const -> RegistryEntityData {
    auto const& state{simulation()};
    RegistryEntityData entity_data;
    ml::append(entity_data.locations, GetActorLocation());

    entity_data.velocities.add(FVector3f{state.velocity});

    entity_data.radii.Add(ml::get_mesh_sphere_bounds(*ship_mesh));
    entity_data.healths.Add(state.health.health);
    entity_data.teams.Add(state.team);
    entity_data.alive.Add(static_cast<uint8>(is_alive()));
    entity_data.entity_types.Add(ETestEntityType::PlayerShip);

    return entity_data;
}
auto ATestSpaceShip::get_unique_id() const -> TestEntityUniqueId {
    return simulation().unique_entity_id;
}
auto ATestSpaceShip::get_entity_registry_handle() const -> FRegistryEntityHandle {
    return simulation().registry_handle;
}
auto ATestSpaceShip::get_team() const noexcept -> ETestTeam {
    return simulation().team;
}
void ATestSpaceShip::set_team(ETestTeam const new_team) noexcept {
    team = new_team;
    if (bound_simulation) {
        bound_simulation->team = new_team;
    }
    if (standalone_simulation.IsSet()) {
        standalone_simulation->team = new_team;
    }
}
auto ATestSpaceShip::get_kills() const -> int32 {
    return entity_registry->get_kills(simulation().unique_entity_id);
}

/* ------------------------------------------------------------------------------------------ */
// Movement
/* ------------------------------------------------------------------------------------------ */
void ATestSpaceShip::integrate_velocity(float const dt) {
    auto& state{simulation()};
    switch (state.flight_mode) {
        case ETestSpaceShipFlightMode::ForwardSpeed: {
            auto const fwd{GetActorForwardVector()};
            auto const new_speed{state.forward_flight_model.update(dt)};
            state.velocity = fwd * new_speed;
            break;
        }
        case ETestSpaceShipFlightMode::PlanarVelocity: {
            state.planar_velocity = state.planar_flight_model.update(dt);
            state.velocity = state.planar_velocity;
            break;
        }
    }

    on_speed_changed.ExecuteIfBound(state.velocity.Size());
}
auto ATestSpaceShip::get_velocity() const -> FVector {
    return simulation().velocity;
}
auto ATestSpaceShip::GetVelocity() const -> FVector {
    return get_velocity();
}
auto ATestSpaceShip::get_target_speed() const -> float {
    return simulation().target_speed;
}
auto ATestSpaceShip::get_speed() const -> float {
    return get_velocity().Size();
}
void ATestSpaceShip::set_flight_mode(ETestSpaceShipFlightMode const new_flight_mode) noexcept {
    simulation().flight_mode = new_flight_mode;
}

// Movement - turning
void ATestSpaceShip::set_move_input(FVector2D const input) {
    simulation().planar_movement_direction = input;
}
void ATestSpaceShip::set_lateral_move_input(float const input) {
    simulation().planar_movement_direction.X = input;
}
void ATestSpaceShip::set_vertical_move_input(float const input) {
    simulation().planar_movement_direction.Y = input;
}
void ATestSpaceShip::set_ship_2d_control(FVector2D const input) {
    auto& state{simulation()};
    if (!state.sampling) {
        return;
    }

    switch (state.control_mode) {
        case ETestSpaceShipControlMode::Velocity: {
            state.target_local_planar_velocity_scale = input;
            break;
        }
        case ETestSpaceShipControlMode::Power: {
            break;
        }
    }
}
void ATestSpaceShip::set_ship_1d_control_x(float const input) {
    auto control{simulation().target_local_planar_velocity_scale};
    control.X = input;
    set_ship_2d_control(control);
}
void ATestSpaceShip::set_ship_1d_control_y(float const input) {
    auto control{simulation().target_local_planar_velocity_scale};
    control.Y = input;
    set_ship_2d_control(control);
}
void ATestSpaceShip::select_next_control_mode() {
    auto& state{simulation()};
    state.control_mode = ml::get_next(state.control_mode);
}
void ATestSpaceShip::select_previous_control_mode() {
    auto& state{simulation()};
    state.control_mode = ml::get_previous(state.control_mode);
}
void ATestSpaceShip::start_sampling() {
    simulation().sampling = true;
}
void ATestSpaceShip::stop_sampling() {
    auto& state{simulation()};
    state.sampling = false;
    auto const& speed_responses{actor_config->speed_responses};

    switch (state.control_mode) {
        case ETestSpaceShipControlMode::Velocity: {
            auto const fwd{GetActorForwardVector()};
            auto const right{GetActorRightVector()};
            FVector const world_direction{fwd * state.target_local_planar_velocity_scale.Y +
                                          right * state.target_local_planar_velocity_scale.X};

            state.target_local_planar_velocity = world_direction * actor_config->cruise_speed;

            state.planar_flight_model.set_new_impulse(speed_responses.accelerating_to_cruise,
                                                      state.planar_velocity,
                                                      state.target_local_planar_velocity);
            break;
        }
        case ETestSpaceShipControlMode::Power: {
            break;
        }
    }
}
void ATestSpaceShip::turn(FVector2D direction) {
#if WITH_EDITOR
    if (log_config.can_log(EActorLogVerbosity::VeryVerbose)) {
        UE_LOG(LogSandbox, Verbose, TEXT("Turning: %s"), *direction.ToString());
    }
#endif

    simulation().rotation_input = direction;
}
void ATestSpaceShip::update_actor_rotation(float const dt) {
    auto& state{simulation()};
    auto const rotation_speed{actor_config->rotation_speed};
    auto const d_rot{rotation_speed * dt};

    if (state.rotation_input != FVector2D::ZeroVector || !FMath::IsNearlyZero(state.roll_input)) {
        auto const drot_pitch{d_rot};

        auto const abs_yaw_strength{FMath::Abs(state.rotation_input.X)};
        auto const yaw_speed{actor_config->rotation_speed * abs_yaw_strength};
        auto const drot_yaw{yaw_speed * dt};

        auto const d_pitch{state.rotation_input.Y * drot_pitch};
        auto const d_yaw{state.rotation_input.X * drot_yaw};
        auto const d_roll{state.roll_input * d_rot};

        FRotator const delta_rotation(d_pitch, d_yaw, d_roll);
        AddActorLocalRotation(delta_rotation);

        state.time_since_rotation_input = 0.f;
        return;
    }

    if (state.time_since_rotation_input >= actor_config->auto_level_roll_delay) {
        auto const auto_level_speed{actor_config->auto_level_speed};
        auto const rot{GetActorRotation()};

        auto const roll{FMath::FInterpTo(rot.Roll, 0.0f, dt, auto_level_speed)};

        SetActorRotation(FRotator{rot.Pitch, rot.Yaw, roll});
    }
}

// Movement - boost/brake
void ATestSpaceShip::set(EBoostBrakeState s) {
    auto& state{simulation()};
    auto const cur_speed{get_speed()};
    auto const& speed_responses{actor_config->speed_responses};
    FSpeedResponse response{speed_responses.accelerating_to_cruise};

    switch (s) {
        case EBoostBrakeState::Boost: {
            state.target_speed = actor_config->boost_speed;
            state.thrust_change_rate = -(1.f / actor_config->boost_depletion_time);
            response = speed_responses.boost;
            boost_pulse->Activate();

            boost_engine_effect->Activate();
            break;
        }
        case EBoostBrakeState::Brake: {
            state.target_speed = actor_config->brake_speed;
            state.thrust_change_rate = -(1.f / actor_config->brake_depletion_time);
            response = speed_responses.brake;
            break;
        }
        default:
            UE_LOG(LogSandbox, Error, TEXT("Unhandled state."));
            [[fallthrough]];
        case EBoostBrakeState::None: {
            state.target_speed = actor_config->cruise_speed;
            state.thrust_change_rate = 1.f / actor_config->thrust_recharge_time;
            if (state.target_speed < cur_speed) {
                response = speed_responses.slowing_to_cruise;
            }

            boost_engine_effect->Deactivate();

            break;
        }
    }

    on_target_speed_changed.ExecuteIfBound(state.target_speed);
    state.forward_flight_model.set_new_impulse(response, cur_speed, state.target_speed);
    state.boost_brake_state = s;
}

void ATestSpaceShip::start_boost() {
    if (energy_is_full() && (simulation().boost_brake_state == EBoostBrakeState::None)) {
        set(EBoostBrakeState::Boost);
    }
}
void ATestSpaceShip::stop_boost() {
    if (simulation().boost_brake_state == EBoostBrakeState::Boost) {
        set(EBoostBrakeState::None);
    }
}
void ATestSpaceShip::start_brake() {
    if (energy_is_full() && (simulation().boost_brake_state == EBoostBrakeState::None)) {
        set(EBoostBrakeState::Brake);
    }
}
void ATestSpaceShip::stop_brake() {
    if (simulation().boost_brake_state == EBoostBrakeState::Brake) {
        set(EBoostBrakeState::None);
    }
}
void ATestSpaceShip::update_boost_brake(float const dt) {
    auto& state{simulation()};
    auto const starting_thrust_energy{state.thrust_energy};

    if (starting_thrust_energy <= 0.f) {
        set(EBoostBrakeState::None);
    }

    state.thrust_energy += dt * state.thrust_change_rate;
    state.thrust_energy = FMath::Clamp(state.thrust_energy, 0.f, actor_config->thrust_energy_max);

    if (starting_thrust_energy != state.thrust_energy) {
        on_energy_changed.ExecuteIfBound(state.thrust_energy / actor_config->thrust_energy_max);
    }
}

// Movement - rolling
void ATestSpaceShip::roll(float direction) {
    simulation().roll_input = FMath::Clamp(direction, -1.f, 1.f);
}

/* ------------------------------------------------------------------------------------------ */
/* Combat */
/* ------------------------------------------------------------------------------------------ */
void ATestSpaceShip::set_lock_on_target(FRegistryEntityHandle const target) {
    simulation().lock_on_target = target;
}

// Combat - laser
void ATestSpaceShip::set_laser_mode(ELaserFiringState new_laser_mode) {
    auto& state{simulation()};
    if (state.laser_firing_mode != new_laser_mode) {
        on_laser_mode_changed.ExecuteIfBound(new_laser_mode);
    }
    state.laser_firing_mode = new_laser_mode;
}
void ATestSpaceShip::update_laser_firing() {
    auto& state{simulation()};
    auto const cooldown_finished{state.laser_shot_cooldown <= 0.f};

    switch (state.laser_firing_mode) {
        case ELaserFiringState::idle: {
            break;
        }
        case ELaserFiringState::burst: {
            if (cooldown_finished) {
                fire_laser();
                state.laser_shot_cooldown = actor_config->laser.fire_cooldown;

                if (state.lasers_fired_this_burst >= state.lasers_per_burst) {
                    state.laser_shot_cooldown = actor_config->laser_lock_on_transition_delay;
                    set_laser_mode(ELaserFiringState::lock_on_transition);
                }
            }
            break;
        }
        case ELaserFiringState::lock_on_transition: {
            if (cooldown_finished) {
                set_laser_mode(ELaserFiringState::lock_on_searching);
            }
        }
        case ELaserFiringState::lock_on_searching: {
            check(spatial_query_manager);
            TRY_INIT_PTR(world, GetWorld());
            auto const middle{get_middle_socket()};

            auto const start{middle.GetLocation()};
            auto const fwd{middle.Rotator().Vector()};
            auto const distance{actor_config->laser_lock_on_distance};
            auto const end{start + fwd * distance};

            auto const hit{spatial_query_manager->trace_closest(
                FVector3f{start}, FVector3f{end}, state.registry_handle)};
            if (hit.hit && hit.entity.is_valid()) {
                set_lock_on_target(hit.entity);
                set_laser_mode(ELaserFiringState::lock_on_acquired);
            }

#if WITH_EDITOR
            if (debug_lock_on) {
                DrawDebugLine(world, start, end, FColor::Green, false, 0.0f, 0, 10.0f);
                DrawDebugSphere(world, end, debug_lock_on_sphere_radius, 8, FColor::Orange);
            }
#endif

            break;
        }
        case ELaserFiringState::lock_on_acquired: {
            break;
        }
    }
}
void ATestSpaceShip::start_fire_laser() {
    set_laser_mode(ELaserFiringState::burst);
    auto& state{simulation()};
    state.lasers_fired_this_burst = 0;
    state.laser_shot_cooldown = 0.f;
    set_lock_on_target({});
}
void ATestSpaceShip::stop_fire_laser() {
    if (simulation().laser_firing_mode == ELaserFiringState::lock_on_acquired) {
        set_lock_on_target({});
    }

    set_laser_mode(ELaserFiringState::idle);
}
void ATestSpaceShip::fire_laser() {
    auto& state{simulation()};
    switch (state.laser_mode) {
        case EShipLaserMode::Single: {
            TStaticArray<FTransform, 1> fire_points{
                get_middle_socket(),
            };
            fire_lasers_from(fire_points);
            break;
        }
        case EShipLaserMode::Double: {
            TStaticArray<FTransform, 2> fire_points{
                ship_mesh->GetSocketTransform(Sockets::left, RTS_World),
                ship_mesh->GetSocketTransform(Sockets::right, RTS_World),
            };
            fire_lasers_from(fire_points);
            break;
        }
        case EShipLaserMode::Hyper: {
            TStaticArray<FTransform, 2> fire_points{
                ship_mesh->GetSocketTransform(Sockets::left, RTS_World),
                ship_mesh->GetSocketTransform(Sockets::right, RTS_World),
            };
            fire_lasers_from(fire_points);
            break;
        }
        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("Unhandled fire_laser branch."));
            break;
        }
    }

    state.lasers_fired_this_burst++;
    state.laser_shot_cooldown = actor_config->laser.fire_cooldown;
}
void ATestSpaceShip::fire_lasers_from(TConstArrayView<FTransform> const fire_points) {
    ml::test_lasers::SpawnRequests new_lasers;

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};

    auto const n{fire_points.Num()};
    ml::add_uninitialised(n, new_lasers);

    for (int32 i{0}; i < n; ++i) {
        ml::assign(new_lasers.locations, i, fire_points[i].GetLocation());
        ml::assign(new_lasers.rotations, i, fire_points[i].Rotator());
        ml::assign(new_lasers.base_velocities, i, FVector3f{get_velocity()});
    }

    new_lasers.set_damages(actor_config->laser.damage);
    new_lasers.set_speeds(actor_config->laser.projectile_speed);
    new_lasers.set_max_distances(actor_config->laser.max_distance);
    new_lasers.set_colours(colour_cache[team]);
    ml::fill(new_lasers.instigator_handles, simulation().registry_handle);

    laser_actor->queue_laser_spawns(new_lasers);
}
void ATestSpaceShip::upgrade_laser() {
    auto& state{simulation()};
    if (state.laser_mode == EShipLaserMode::Single) {
        state.laser_mode = EShipLaserMode::Double;
    } else if (state.laser_mode == EShipLaserMode::Double) {
        state.laser_mode = EShipLaserMode::Hyper;
    }
}

void ATestSpaceShip::select_next_laser_fire_rate() noexcept {
    set_laser_fire_rate(ml::get_next(simulation().laser_fire_rate));
}
void ATestSpaceShip::select_previous_laser_fire_rate() noexcept {
    set_laser_fire_rate(ml::get_previous(simulation().laser_fire_rate));
}
void ATestSpaceShip::set_laser_fire_rate(ETestShipFireRate const value) noexcept {
    auto& state{simulation()};
    state.laser_fire_rate = value;

    switch (state.laser_fire_rate) {
        case ETestShipFireRate::Single: {
            state.lasers_per_burst = 1;
            break;
        }
        case ETestShipFireRate::Burst3: {
            state.lasers_per_burst = 3;
            break;
        }
        case ETestShipFireRate::FullAuto: {
            state.lasers_per_burst = std::numeric_limits<decltype(state.lasers_per_burst)>::max();
            break;
        }
    }

    on_ship_fire_rate_changed.ExecuteIfBound(state.laser_fire_rate);
}

/* ------------------------------------------------------------------------------------------ */
// Visuals
/* ------------------------------------------------------------------------------------------ */
void ATestSpaceShip::update_visual_orientation(float const dt) {
    auto const& state{simulation()};
    auto const current_rotation{ship_mesh->GetRelativeRotation()};

    auto const target_pitch{state.rotation_input.Y * actor_config->pitch_angle_max};
    auto const new_pitch{
        FMath::FInterpTo(current_rotation.Pitch, target_pitch, dt, actor_config->pitch_speed)};

    auto const target_yaw{state.rotation_input.X * actor_config->yaw_angle_max};
    auto const new_yaw{
        FMath::FInterpTo(current_rotation.Yaw, target_yaw, dt, actor_config->yaw_speed)};

    auto const turn_intensity{state.rotation_input.X};
    auto const turn_target{turn_intensity * actor_config->turn_bank_angle_max};
    auto const turn_speed{turn_intensity * actor_config->turn_bank_speed};

    auto const roll_speed{FMath::Max(actor_config->turn_bank_speed, FMath::Abs(turn_speed))};
    auto const new_roll{FMath::FInterpTo(current_rotation.Roll, turn_target, dt, roll_speed)};

    ship_mesh->SetRelativeRotation(FRotator(new_pitch, new_yaw, new_roll));
}

void ATestSpaceShip::configure_boost_pulse() {
    boost_pulse->SetColorParameter(TEXT("colour"), actor_config->engine_colour);
    boost_pulse->SetFloatParameter(TEXT("ring_colour_intensity"),
                                   actor_config->boost_effect_colour_intensity);
    boost_pulse->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                   actor_config->boost_effect_colour_intensity);
}
void ATestSpaceShip::configure_boost_engine_effect() {
    boost_engine_effect->SetColorParameter(TEXT("colour"), actor_config->engine_colour);
    boost_engine_effect->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                           actor_config->boost_effect_colour_intensity);
}
void ATestSpaceShip::configure_ship_mesh() {
    ship_mesh->SetCanEverAffectNavigation(false);
    ship_mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ship_mesh->SetGenerateOverlapEvents(false);
}

// Mesh
auto ATestSpaceShip::get_middle_socket() const -> FTransform {
    check(ship_mesh);
    return get_middle_socket(*ship_mesh);
}
auto ATestSpaceShip::get_middle_socket(UStaticMeshComponent const& m) const -> FTransform {
    check(m.DoesSocketExist(Sockets::middle));
    return m.GetSocketTransform(Sockets::middle, RTS_World);
}
auto ATestSpaceShip::get_ship_forward_vector() const -> FVector {
    return get_middle_socket().GetLocation();
}

/* ------------------------------------------------------------------------------------------ */
// Health
/* ------------------------------------------------------------------------------------------ */
void ATestSpaceShip::add_health(int32 added_health) {
    set_health(simulation().health.health + added_health);
}
void ATestSpaceShip::set_health(int32 new_health) {
    auto& state{simulation()};
    if (new_health == state.health.health) {
        return;
    }

    auto const was_alive{is_alive()};
    state.health.health = FMath::Min(new_health, state.health.max_health);
    on_health_changed.ExecuteIfBound(state.health);

    if (was_alive && !is_alive()) {
        die({});
    }
}
void ATestSpaceShip::die(FRegistryEntityHandle const killer) {
    EntityDeathInfo death_info;
    auto const reason{killer.is_null() ? ETestDeathReason::Unknown : ETestDeathReason::Combat};
    death_info.add(reason, simulation().registry_handle, killer);
    queue_entity_update(death_info);

    // The callback unpossesses and unbinds from this actor, so execute a local delegate.
    auto player_ship_died{MoveTemp(on_player_ship_died)};
    player_ship_died.ExecuteIfBound();
    Destroy();
}

/* ------------------------------------------------------------------------------------------ */
// Energy
/* ------------------------------------------------------------------------------------------ */
bool ATestSpaceShip::energy_is_full() const {
    return simulation().thrust_energy == actor_config->thrust_energy_max;
}
auto ATestSpaceShip::get_energy() const -> float {
    check(actor_config);
    check(actor_config->thrust_energy_max > 0.f);
    return simulation().thrust_energy / actor_config->thrust_energy_max;
}

/* ------------------------------------------------------------------------------------------ */
// Debugging
/* ------------------------------------------------------------------------------------------ */
#if WITH_EDITOR
void ATestSpaceShip::sample_speed() {
    auto& state{simulation()};
    state.speed_samples[state.speed_sample_index] = {
        FMath::Clamp(simulation_clock.get_simulation_time(), 0.0, 1e9),
        FMath::Clamp(state.velocity.Size(), 0.0, 100e3)};
    state.speed_sample_index++;
    if (state.speed_sample_index >= state.speed_sample_max) {
        state.speed_sample_index = 0;
    }

    on_speed_sampled.ExecuteIfBound(TConstArrayView<FVector2d>{state.speed_samples},
                                    state.speed_sample_index);
}
#endif
void ATestSpaceShip::draw_debug_shapes() {
    if (debug_forward_socket_direction) {
        auto const middle{get_middle_socket(*ship_mesh)};

        FVector const start = middle.GetLocation();
        FVector const forward = middle.GetUnitAxis(EAxis::X);
        constexpr float len{5000.f};
        FVector const end = start + forward * len;
        DrawDebugLine(GetWorld(), start, end, FColor::Green, false, 0.0f, 0, 10.0f);
    }

    if (debug_forward_direction) {
        auto const fwd{GetActorForwardVector()};
        auto const start{GetActorLocation()};
        constexpr float len{5000.f};
        FVector const end = start + fwd * len;
        DrawDebugLine(GetWorld(), start, end, FColor::Green, false, 0.0f, 0, 10.0f);
    }
}
void ATestSpaceShip::configure_speed_sampling() {
#if WITH_EDITOR
    auto& state{simulation()};
    static constexpr double sample_rate_hz{60.0};
    static constexpr double sample_window_seconds{5.0};
    auto const sample_tick_period{simulation_clock.frequency_to_tick_period(sample_rate_hz)};
    auto const sample_window_ticks{simulation_clock.duration_to_tick_period(sample_window_seconds)};
    check(std::in_range<int32>(sample_tick_period));
    check(sample_tick_period > 0);
    auto const sample_count{(sample_window_ticks + sample_tick_period - 1) / sample_tick_period};
    check(std::in_range<int32>(sample_count));

    state.speed_sample_index = 0;
    state.speed_sample_max = static_cast<int32>(sample_count);
    state.speed_sample_tick_period = static_cast<int32>(sample_tick_period);
    state.speed_sample_ticks_remaining = state.speed_sample_tick_period;
    state.speed_samples.Reserve(state.speed_sample_max);
    for (int32 i{0}; i < state.speed_sample_max; ++i) {
        state.speed_samples.Add(FVector2d::ZeroVector);
    }

#endif
}

void ATestSpaceShip::bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) {
    simulation_clock.bind(orchestrator);
}
