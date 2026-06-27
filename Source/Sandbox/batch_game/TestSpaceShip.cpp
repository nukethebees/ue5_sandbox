#include "TestSpaceShip.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestSpaceShipData.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/combat/weapons/ShipBomb.h>
#include <Sandbox/combat/weapons/ShipLaserConfig.h>
#include <Sandbox/constants/collision_channels.h>
#include <Sandbox/health/ShipHealthComponent.h>
#include <Sandbox/logging/SandboxLogCategories.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/uobject_utils.h>

#include <Camera/CameraComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <NiagaraComponent.h>
#include <TimerManager.h>

#include "Sandbox/utilities/macros/null_checks.hpp"

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

void ATestSpaceShip::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::begin_play);

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(ship_config),
        SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
        SANDBOX_NAMED_UOBJECT_PTR(ship_mesh),
        SANDBOX_NAMED_UOBJECT_PTR(boost_pulse),
    });

    velocity = GetActorForwardVector() * ship_config->cruise_speed;
    thrust_energy = ship_config->thrust_energy_max;

    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::left));
    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::right));
    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::middle));

    set_laser_mode(ELaserFiringMode::idle);

    configure_speed_sampling();

    thrust_change_rate = thrust_change_rate;
    velocity = FVector3d::ZeroVector;
    set(EBoostBrakeState::None);

    configure_boost_pulse();
    configure_boost_engine_effect();

    register_with_entity_registry();

    health.clamp_to_max();
}
void ATestSpaceShip::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::begin_tick);
}
void ATestSpaceShip::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::tick);

    log_config.tick(dt);

    laser_shot_cooldown -= dt;
    time_since_rotation_input += dt;
    update_barrel_roll_timers(dt);
}
void ATestSpaceShip::move(float const dt) {
    update_boost_brake(dt);
    update_actor_rotation(dt);
    update_visual_orientation(dt);
    integrate_velocity(dt);
}
void ATestSpaceShip::queue_commands() {
    update_laser_firing();
}
void ATestSpaceShip::resolve_hit_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::resolve_hit_events);

    auto const view{entity_registry->get_damage_queue_view(owner_id)};
    auto const n{view.num()};

    auto const original_health{health.health};

    for (int32 i{0}; i < n; ++i) {
        auto const ismc_index_hit{view.hit_items[i]};
        health.health -= view.damage_amounts[i];
    }

    if (health.health != original_health) { on_health_changed.ExecuteIfBound(health); }
}
void ATestSpaceShip::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::update_entity_registry);

    entity_registry->update_entities(ATestEntityRegistry::ConstView{
        {&registry_handle, 1},
        get_entity_update_data().get_const_view(),
    });
}
void ATestSpaceShip::resolve_damage_targets() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::resolve_damage_targets);
}
void ATestSpaceShip::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::sync_from_registry);
}
void ATestSpaceShip::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::update_visuals);
    boost_engine_effect->SetVectorParameter(TEXT("ship_velocity"), velocity);
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
    registry_handle = new_entities.registry_handles[0];
    unique_entity_id = new_entities.first_id;

    check(entity_registry->is_valid_unique_id(unique_entity_id));

    update_entity_registry();
}
auto ATestSpaceShip::get_entity_update_data() const -> FTestEntityRegistryEntityData {
    FTestEntityRegistryEntityData entity_data;
    ml::append(entity_data.locations, GetActorLocation());

    entity_data.velocities.xs.Add(velocity.X);
    entity_data.velocities.ys.Add(velocity.Y);
    entity_data.velocities.zs.Add(velocity.Z);

    entity_data.healths.Add(health.health);
    entity_data.teams.Add(team);
    entity_data.alive.Add(1);

    return entity_data;
}
void ATestSpaceShip::set_owner_id(TestEntityOwnerId const new_owner_id) {
    owner_id = new_owner_id;
}
auto ATestSpaceShip::get_owner_id() const -> TestEntityOwnerId {
    return owner_id;
}
auto ATestSpaceShip::get_unique_id() const -> TestEntityUniqueId {
    return unique_entity_id;
}
auto ATestSpaceShip::get_entity_registry_handle() const -> FRegistryEntityHandle {
    return registry_handle;
}
auto ATestSpaceShip::get_team() const noexcept -> ETestTeam {
    return team;
}

/* ------------------------------------------------------------------------------------------ */
// Movement
/* ------------------------------------------------------------------------------------------ */
void ATestSpaceShip::integrate_velocity(this ATestSpaceShip& self, float const dt) {
    auto const fwd{self.GetActorForwardVector()};
    auto const new_speed{self.flight_model.update_y(dt)};
    self.velocity = fwd * new_speed;

    auto const cur_pos{self.GetActorLocation()};
    auto const delta_pos{self.velocity * dt};

    self.SetActorLocation(cur_pos + delta_pos, true);
    self.on_speed_changed.ExecuteIfBound(new_speed);
}
auto ATestSpaceShip::GetVelocity() const -> FVector {
    return get_velocity();
}

// Movement - turning
void ATestSpaceShip::turn(FVector2D direction) {
#if WITH_EDITOR
    if (log_config.can_log(EActorLogVerbosity::VeryVerbose)) {
        UE_LOG(LogSandbox, Verbose, TEXT("Turning: %s"), *direction.ToString());
    }
#endif

    rotation_input = direction;
}
void ATestSpaceShip::update_actor_rotation(this ATestSpaceShip& self, float const dt) {
    auto const rotation_speed{self.ship_config->rotation_speed};
    auto const d_rot{rotation_speed * dt};

    if (self.rotation_input != FVector2D::ZeroVector) {
        auto const drot_pitch{d_rot};

        auto const manual_bank_strength{self.manual_bank_direction * 0.5};
        auto const abs_yaw_strength{FMath::Abs(self.rotation_input.X + manual_bank_strength)};
        auto const yaw_speed{self.ship_config->rotation_speed * abs_yaw_strength};
        auto const drot_yaw{yaw_speed * dt};

        auto const d_pitch{self.rotation_input.Y * drot_pitch};
        auto const d_yaw{self.rotation_input.X * drot_yaw};
        auto const d_roll{0.f};

        FRotator const delta_rotation(d_pitch, d_yaw, d_roll);
        self.AddActorLocalRotation(delta_rotation);

        self.time_since_rotation_input = 0.f;
        return;
    }

    if (self.time_since_rotation_input >= self.ship_config->auto_level_roll_delay) {
        auto const auto_level_speed{self.ship_config->auto_level_speed};
        auto const rot{self.GetActorRotation()};

        auto const roll{FMath::FInterpTo(rot.Roll, 0.0f, dt, auto_level_speed)};

        self.SetActorRotation(FRotator{rot.Pitch, rot.Yaw, roll});
    }
}

// Movement - boost/brake
void ATestSpaceShip::set(EBoostBrakeState s) {
    auto const cur_speed{get_speed()};
    FSpeedResponse response{speed_responses.accelerating_to_cruise};

    switch (s) {
        case EBoostBrakeState::Boost: {
            target_speed = ship_config->boost_speed;
            thrust_change_rate = -(1.f / ship_config->boost_depletion_time);
            response = speed_responses.boost;
            boost_pulse->Activate();

            boost_engine_effect->Activate();
            break;
        }
        case EBoostBrakeState::Brake: {
            target_speed = ship_config->brake_speed;
            thrust_change_rate = -(1.f / ship_config->brake_depletion_time);
            response = speed_responses.brake;
            break;
        }
        default:
            UE_LOG(LogSandbox, Error, TEXT("Unhandled state."));
            [[fallthrough]];
        case EBoostBrakeState::None: {
            target_speed = ship_config->cruise_speed;
            thrust_change_rate = 1.f / ship_config->thrust_recharge_time;
            if (target_speed < cur_speed) { response = speed_responses.slowing_to_cruise; }

            boost_engine_effect->Deactivate();

            break;
        }
    }

    flight_model.set_new_impulse(response, cur_speed, target_speed);
    boost_brake_state = s;
}

void ATestSpaceShip::start_boost() {
    if (energy_is_full() && (boost_brake_state == EBoostBrakeState::None)) {
        set(EBoostBrakeState::Boost);
    }
}
void ATestSpaceShip::stop_boost() {
    if (boost_brake_state == EBoostBrakeState::Boost) { set(EBoostBrakeState::None); }
}
void ATestSpaceShip::start_brake() {
    if (energy_is_full() && (boost_brake_state == EBoostBrakeState::None)) {
        set(EBoostBrakeState::Brake);
    }
}
void ATestSpaceShip::stop_brake() {
    if (boost_brake_state == EBoostBrakeState::Brake) { set(EBoostBrakeState::None); }
}
void ATestSpaceShip::update_boost_brake(this ATestSpaceShip& self, float const dt) {
    auto const starting_thrust_energy{self.thrust_energy};

    if (starting_thrust_energy <= 0.f) { self.set(EBoostBrakeState::None); }

    self.thrust_energy += dt * self.thrust_change_rate;
    self.thrust_energy = FMath::Clamp(self.thrust_energy, 0.f, self.ship_config->thrust_energy_max);

    if (starting_thrust_energy != self.thrust_energy) {
        self.on_energy_changed.ExecuteIfBound(self.thrust_energy /
                                              self.ship_config->thrust_energy_max);
    }
}

// Movement - rolling
void ATestSpaceShip::update_barrel_roll_timers(float const dt) {
    auto const original_roll_time{roll_state.roll_time_remaining};

    roll_state.roll_time_remaining -= dt;
    roll_state.cooldown_remaining -= dt;

    if ((original_roll_time >= 0.f) && (roll_state.roll_time_remaining <= 0.f)) {
        roll_state.cooldown_remaining = ship_config->barrel_roll_config.roll_cooldown;
    }
}
void ATestSpaceShip::roll(float direction) {
#if WITH_EDITOR
    if (log_config.can_log(EActorLogVerbosity::VeryVerbose)) {
        UE_LOG(LogSandbox, Verbose, TEXT("Rolling: %.2f"), direction);
    }
#endif
    manual_bank_direction = clamp(direction, 1.f);
}
void ATestSpaceShip::barrel_roll(float direction) {
    if (!roll_state.can_start_rolling()) { return; }

    roll_state.roll_time_remaining = ship_config->barrel_roll_config.roll_duration;
    roll_state.direction = FMath::Sign(direction);

#if WITH_EDITOR
    UE_LOG(LogSandbox, Verbose, TEXT("Barrel rolling: %.2f"), direction);
#endif
}

/* ------------------------------------------------------------------------------------------ */
/* Combat */
/* ------------------------------------------------------------------------------------------ */
void ATestSpaceShip::set_lock_on_target(AActor* target) {
    lock_on_target = target;
    on_lock_on_acquired.ExecuteIfBound(lock_on_target);
}

// Combat - laser
void ATestSpaceShip::set_laser_mode(ELaserFiringMode new_laser_mode) {
    if (laser_firing_mode != new_laser_mode) {
        on_laser_mode_changed.ExecuteIfBound(new_laser_mode);
    }
    laser_firing_mode = new_laser_mode;
}
void ATestSpaceShip::update_laser_firing() {
    auto const cooldown_finished{laser_shot_cooldown <= 0.f};

    switch (laser_firing_mode) {
        case ELaserFiringMode::idle: {
            break;
        }
        case ELaserFiringMode::burst: {
            if (cooldown_finished) {
                fire_laser();
                laser_shot_cooldown = ship_config->laser_firing_period;

                if (lasers_fired_this_burst >= ship_config->lasers_per_burst) {
                    laser_shot_cooldown = ship_config->laser_lock_on_transition_delay;
                    set_laser_mode(ELaserFiringMode::lock_on_transition);
                }
            }
            break;
        }
        case ELaserFiringMode::lock_on_transition: {
            if (cooldown_finished) { set_laser_mode(ELaserFiringMode::lock_on_searching); }
        }
        case ELaserFiringMode::lock_on_searching: {
            TRY_INIT_PTR(world, GetWorld());
            auto const middle{get_middle_socket()};

            auto const start{middle.GetLocation()};
            auto const fwd{middle.Rotator().Vector()};
            auto const distance{ship_config->laser_lock_on_distance};
            auto const end{start + fwd * distance};

            FHitResult hit;
            FCollisionQueryParams query_params;
            FCollisionObjectQueryParams obj_query_params;
            obj_query_params.AddObjectTypesToQuery(ECollisionChannel::ECC_Pawn);

            if (world->LineTraceSingleByObjectType(
                    hit, start, end, obj_query_params, query_params)) {

                auto const actor_hit{hit.GetActor()};
                if (!actor_hit) { break; }

                set_lock_on_target(hit.GetActor());
                set_laser_mode(ELaserFiringMode::lock_on_acquired);
            }

#if WITH_EDITOR
            if (debug_lock_on) {
                DrawDebugLine(world, start, end, FColor::Green, false, 0.0f, 0, 10.0f);
                DrawDebugSphere(world, end, debug_lock_on_sphere_radius, 8, FColor::Orange);
            }
#endif

            break;
        }
        case ELaserFiringMode::lock_on_acquired: {
            break;
        }
    }
}
void ATestSpaceShip::start_fire_laser() {
    set_laser_mode(ELaserFiringMode::burst);
    lasers_fired_this_burst = 0;
    laser_shot_cooldown = 0.f;
    set_lock_on_target(nullptr);
}
void ATestSpaceShip::stop_fire_laser() {
    if (laser_firing_mode == ELaserFiringMode::lock_on_acquired) {
        fire_homing_laser();
        set_lock_on_target(nullptr);
    }

    set_laser_mode(ELaserFiringMode::idle);
}
void ATestSpaceShip::fire_laser() {
    switch (laser_mode) {
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

    lasers_fired_this_burst++;
    laser_shot_cooldown = ship_config->laser_firing_period;
}
void ATestSpaceShip::fire_lasers_from(TConstArrayView<FTransform> const fire_points) {
    FTestLasersSpawnRequests new_lasers;

    auto const n{fire_points.Num()};
    ml::add_uninitialised(n, new_lasers);

    for (int32 i{0}; i < n; ++i) {
        ml::assign(new_lasers.locations, i, fire_points[i].GetLocation());
        ml::assign(new_lasers.rotations, i, fire_points[i].Rotator());
    }

    new_lasers.set_damages(ship_config->laser_damage);
    new_lasers.set_speeds(ship_config->laser_speed);
    new_lasers.set_max_distances(ship_config->laser_max_distance);
    new_lasers.set_colours(FLinearColor::Green);
    ml::fill(new_lasers.instigator_handles, registry_handle);

    laser_actor->spawn_lasers(new_lasers);
}
void ATestSpaceShip::upgrade_laser() {
    if (laser_mode == EShipLaserMode::Single) {
        laser_mode = EShipLaserMode::Double;
    } else if (laser_mode == EShipLaserMode::Double) {
        laser_mode = EShipLaserMode::Hyper;
    }
}

// Combat - bomb
void ATestSpaceShip::fire_bomb() {
    if (auto ab{active_bomb.Pin()}) {
        if (!ab->has_detonated()) {
            active_bomb.Get()->detonate();
            return;
        }
    }

    if (bombs <= 0) {
        UE_LOG(LogSandbox, Verbose, TEXT("No bombs left."));
        return;
    }

    TRY_INIT_PTR(world, GetWorld());
    auto const fire_point{ship_mesh->GetSocketTransform(Sockets::middle, RTS_World)};

    UE_LOG(LogSandbox, Verbose, TEXT("Spawning bomb at %s"), *fire_point.ToHumanReadableString());

    active_bomb =
        world->SpawnActorDeferred<AShipBomb>(ship_config->bomb_class, fire_point, nullptr, this);
    if (laser_firing_mode == ELaserFiringMode::lock_on_acquired) {
        active_bomb->set_target(this->lock_on_target);
        set_lock_on_target(nullptr);
        set_laser_mode(ELaserFiringMode::idle);
    }
    active_bomb->FinishSpawning(fire_point);

    subtract_bomb();
}
void ATestSpaceShip::add_bomb() {
    bombs++;
    on_bombs_changed.ExecuteIfBound(bombs);
}
void ATestSpaceShip::subtract_bomb() {
    bombs--;
    on_bombs_changed.ExecuteIfBound(bombs);
}

// Combat - homing laser
void ATestSpaceShip::fire_homing_laser() {}

/* ------------------------------------------------------------------------------------------ */
// Visuals
/* ------------------------------------------------------------------------------------------ */
void ATestSpaceShip::update_visual_orientation(this ATestSpaceShip& self, float const dt) {
    auto const current_rotation{self.ship_mesh->GetRelativeRotation()};

    auto const target_pitch{self.rotation_input.Y * self.ship_config->pitch_angle_max};
    auto const new_pitch{
        FMath::FInterpTo(current_rotation.Pitch, target_pitch, dt, self.ship_config->pitch_speed)};

    auto const target_yaw{self.rotation_input.X * self.ship_config->yaw_angle_max};
    auto const new_yaw{
        FMath::FInterpTo(current_rotation.Yaw, target_yaw, dt, self.ship_config->yaw_speed)};

    auto const turn_intensity{self.rotation_input.X};
    auto const turn_target{turn_intensity * self.ship_config->turn_bank_angle_max};
    auto const turn_speed{turn_intensity * self.ship_config->turn_bank_speed};

    auto const manual_bank_intensity{self.manual_bank_direction};
    auto const manual_bank_target{manual_bank_intensity * self.ship_config->manual_bank_angle_max};
    auto const manual_bank_speed{manual_bank_intensity * self.ship_config->manual_bank_speed};

    double new_roll{current_rotation.Roll};
    if (self.roll_state.is_rolling()) {
        auto const delta_roll{dt * self.ship_config->barrel_roll_config.roll_speed *
                              self.roll_state.direction};
        new_roll = current_rotation.Roll + delta_roll;
    } else {
        auto const roll_speed{FMath::Max(self.ship_config->turn_bank_speed,
                                         FMath::Abs(turn_speed + manual_bank_speed))};
        auto const bank_is_bigger{FMath::Abs(manual_bank_target) > FMath::Abs(turn_target)};
        auto const roll_target{bank_is_bigger ? manual_bank_target : turn_target};
        new_roll = FMath::FInterpTo(current_rotation.Roll, roll_target, dt, roll_speed);
    }

    self.ship_mesh->SetRelativeRotation(FRotator(new_pitch, new_yaw, new_roll));
}

void ATestSpaceShip::configure_boost_pulse() {
    boost_pulse->SetColorParameter(TEXT("colour"), ship_config->engine_colour);
    boost_pulse->SetFloatParameter(TEXT("ring_colour_intensity"),
                                   ship_config->boost_effect_colour_intensity);
    boost_pulse->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                   ship_config->boost_effect_colour_intensity);
}
void ATestSpaceShip::configure_boost_engine_effect() {
    boost_engine_effect->SetColorParameter(TEXT("colour"), ship_config->engine_colour);
    boost_engine_effect->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                           ship_config->boost_effect_colour_intensity);
}
void ATestSpaceShip::configure_ship_mesh() {
    ship_mesh->SetCanEverAffectNavigation(false);
    ship_mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ship_mesh->SetGenerateOverlapEvents(false);

    FCollisionResponseContainer collision_response;

    constexpr auto block{ECollisionResponse::ECR_Block};

    collision_response.Visibility = block;
    collision_response.WorldDynamic = block;
    collision_response.WorldStatic = block;

    collision_response.SetResponse(ml::collision::projectile, block);
    collision_response.SetResponse(ml::collision::description, block);

    ship_mesh->SetCollisionResponseToChannels(collision_response);
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
    set_health(health.health + added_health);
}
void ATestSpaceShip::set_health(int32 new_health) {
    if (new_health == health.health) { return; }

    auto const old_health{health.max_health};
    health.health = FMath::Min(new_health, health.max_health);
    on_health_changed.ExecuteIfBound(health);
}

/* ------------------------------------------------------------------------------------------ */
// Energy
/* ------------------------------------------------------------------------------------------ */
bool ATestSpaceShip::energy_is_full() const {
    return thrust_energy == ship_config->thrust_energy_max;
}

/* ------------------------------------------------------------------------------------------ */
// Debugging
/* ------------------------------------------------------------------------------------------ */
#if WITH_EDITOR
void ATestSpaceShip::sample_speed() {
    speed_samples[speed_sample_index] = {FMath::Clamp(GetWorld()->GetTimeSeconds(), 0.0, 1e9),
                                         FMath::Clamp(velocity.Size(), 0.0, 100e3)};
    speed_sample_index++;
    if (speed_sample_index >= speed_sample_max) { speed_sample_index = 0; }

    on_speed_sampled.ExecuteIfBound(std::span(speed_samples.GetData(), speed_samples.Num()),
                                    speed_sample_index);
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
    static constexpr float sample_rate_hz{60.0f};
    static constexpr float sample_interval{1.0f / sample_rate_hz};
    static constexpr float sample_window{5.0f};
    speed_sample_index = 0;
    speed_sample_max = static_cast<int32>(sample_rate_hz * sample_window);
    speed_samples.Reserve(speed_sample_max);
    for (int32 i{0}; i < speed_sample_max; ++i) {
        speed_samples.Add(FVector2d::ZeroVector);
    }

    GetWorldTimerManager().SetTimer(
        speed_sample_timer, this, &ThisClass::sample_speed, sample_interval, true);
#endif
}
