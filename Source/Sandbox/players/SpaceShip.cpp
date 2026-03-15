#include "Sandbox/players/SpaceShip.h"

#include "Sandbox/combat/weapons/ShipBomb.h"
#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/health/ShipHealthComponent.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "TimerManager.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

auto FSpaceShipFlightModel::calculate_dy(float t) const -> float {
    auto const z{damping_factor};
    auto const w{natural_angular_frequency()};

    auto const a{FMath::Cos(w * t)};
    auto const b{z / FMath::Sqrt(1 - z * z)};
    auto const c{FMath::Sin(w * t)};
    auto const h{1.f - FMath::Exp(-z * w * t) * (a + b * c)};

    return step_size() * h;
}
auto FSpaceShipFlightModel::update_y(float dt) -> float {
    time += dt;
    return old_speed + calculate_dy(time);
}
auto FSpaceShipFlightModel::set_new_impulse(float old_s, float target_s) {
    old_speed = old_s;
    target_speed = target_s;
    time = 0.f;
}

ASpaceShip::ASpaceShip()
    : camera(CreateDefaultSubobject<UCameraComponent>(TEXT("camera")))
    , ship_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ship_mesh")))
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box")))
    , health(CreateDefaultSubobject<UShipHealthComponent>(TEXT("health"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    camera->SetupAttachment(RootComponent);
    ship_mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);

    // Don't tick until the controller wires up the delegates
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}
void ASpaceShip::Tick(float dt) {
    Super::Tick(dt);

    update_boost_brake(dt);
    update_actor_rotation(dt);
    update_visual_orientation(dt);
    integrate_velocity(dt);

#if WITH_EDITOR
    auto const middle{ship_mesh->GetSocketTransform(Sockets::middle, RTS_World)};

    FVector const start = middle.GetLocation();
    FVector const forward = middle.GetUnitAxis(EAxis::X);
    constexpr float len{5000.f};
    FVector const end = start + forward * len;
    DrawDebugLine(GetWorld(), start, end, FColor::Green, false, 0.0f, 0, 10.0f);

    if (can_log()) {
        seconds_since_last_log = 0.f;
    }
    seconds_since_last_log += dt;
#endif
}

void ASpaceShip::set(EBoostBrakeState s) {
    switch (s) {
        case EBoostBrakeState::None: {
            target_speed = cruise_speed;
            thrust_change_rate = 1.f / thrust_recharge_time;
            break;
        }
        case EBoostBrakeState::Boost: {
            target_speed = boost_speed;
            thrust_change_rate = -(1.f / boost_depletion_time);
            break;
        }
        case EBoostBrakeState::Brake: {
            target_speed = brake_speed;
            thrust_change_rate = -(1.f / brake_depletion_time);
            break;
        }
        default: {
            UE_LOG(LogSandboxActor, Error, TEXT("Unhandled state."));
            target_speed = cruise_speed;
            thrust_change_rate = 1.f / thrust_recharge_time;
            break;
        }
    }

    flight_model.set_new_impulse(get_speed(), target_speed);
    boost_brake_state = s;
}

void ASpaceShip::update_boost_brake(this ASpaceShip& self, float dt) {
    auto const starting_thrust_energy{self.thrust_energy};

    if (starting_thrust_energy <= 0.f) {
        self.set(EBoostBrakeState::None);
    }

    self.thrust_energy += dt * self.thrust_change_rate;
    self.thrust_energy = FMath::Clamp(self.thrust_energy, 0.f, self.thrust_energy_max);
    if (starting_thrust_energy != self.thrust_energy) {
        self.on_energy_changed.Execute(self.thrust_energy / self.thrust_energy_max);
    }
}
void ASpaceShip::update_actor_rotation(this ASpaceShip& self, float dt) {
    auto const drot{self.rotation_speed * dt};
    if (self.rotation_input == FVector2D::ZeroVector) {

        auto const rot{self.GetActorRotation()};
        FRotator const delta_rotation{
            self.tick_clamp(-rot.Pitch, dt, drot), 0.0f, self.tick_clamp(-rot.Roll, dt, drot)};

        self.AddActorLocalRotation(delta_rotation);
    } else {
        auto const drot_pitch{drot};

        auto const manual_bank_strength{self.manual_bank_direction * 0.5};
        auto const abs_yaw_strength{FMath::Abs(self.rotation_input.X + manual_bank_strength)};
        auto const yaw_speed{self.rotation_speed * abs_yaw_strength};
        auto const drot_yaw{yaw_speed * dt};

        auto const d_pitch{self.rotation_input.Y * drot_pitch};
        auto const d_yaw{self.rotation_input.X * drot_yaw};
        auto const d_roll{0.f};

        FRotator const delta_rotation(d_pitch, d_yaw, d_roll);
        self.AddActorLocalRotation(delta_rotation);
    }
}
void ASpaceShip::update_visual_orientation(this ASpaceShip& self, float dt) {
    auto const current_rotation{self.ship_mesh->GetRelativeRotation()};

    auto const target_pitch{self.rotation_input.Y * self.pitch_angle_max};
    auto const new_pitch{
        FMath::FInterpTo(current_rotation.Pitch, target_pitch, dt, self.pitch_speed)};

    auto const target_yaw{self.rotation_input.X * self.yaw_angle_max};
    auto const new_yaw{FMath::FInterpTo(current_rotation.Yaw, target_yaw, dt, self.yaw_speed)};

    auto const turn_intensity{self.rotation_input.X};
    auto const turn_target{turn_intensity * self.turn_bank_angle_max};
    auto const turn_speed{turn_intensity * self.turn_bank_speed};

    auto const manual_bank_intensity{self.manual_bank_direction};
    auto const manual_bank_target{manual_bank_intensity * self.manual_bank_angle_max};
    auto const manual_bank_speed{manual_bank_intensity * self.manual_bank_speed};

    auto const roll_speed{
        FMath::Max(self.turn_bank_speed, FMath::Abs(turn_speed + manual_bank_speed))};
    auto const bank_is_bigger{FMath::Abs(manual_bank_target) > FMath::Abs(turn_target)};
    auto const roll_target{bank_is_bigger ? manual_bank_target : turn_target};
    auto const new_roll{FMath::FInterpTo(current_rotation.Roll, roll_target, dt, roll_speed)};

    self.ship_mesh->SetRelativeRotation(FRotator(new_pitch, new_yaw, new_roll));
}
void ASpaceShip::integrate_velocity(this ASpaceShip& self, float dt) {
    auto const fwd{self.GetActorForwardVector()};
    auto const new_speed{self.flight_model.update_y(dt)};
    self.velocity = fwd * new_speed;

    auto const cur_pos{self.GetActorLocation()};
    auto const delta_pos{self.velocity * dt};

    self.SetActorLocation(cur_pos + delta_pos, true);
    self.on_speed_changed.Execute(new_speed);
}

void ASpaceShip::BeginPlay() {
    Super::BeginPlay();

    velocity = GetActorForwardVector() * cruise_speed;
    thrust_energy = thrust_energy_max;

    check(ship_mesh);
    RETURN_IF_NULLPTR(ship_mesh);
    check(laser_class);
    RETURN_IF_NULLPTR(laser_class);
    check(laser_config);
    RETURN_IF_NULLPTR(laser_config);
    check(hyper_laser_config);
    RETURN_IF_NULLPTR(hyper_laser_config);

    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::left));
    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::right));
    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::middle));

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

    // Coeff 1 is chosen by the user
    drag_coeff_2 =
        (cruise_acceleration - drag_coeff_1 * cruise_speed) / (cruise_speed * cruise_speed);

    thrust_change_rate = thrust_change_rate;
    velocity = FVector3d::ZeroVector;
    set(EBoostBrakeState::None);
}

void ASpaceShip::turn(FVector2D direction) {
#if WITH_EDITOR
    if (can_log()) {
        UE_LOG(LogSandboxActor, Verbose, TEXT("Turning: %s"), *direction.ToString());
    }
#endif

    rotation_input = direction;
}
void ASpaceShip::start_boost() {
    if (energy_is_full() && (boost_brake_state == EBoostBrakeState::None)) {
        set(EBoostBrakeState::Boost);
    }
}
void ASpaceShip::stop_boost() {
    if (boost_brake_state == EBoostBrakeState::Boost) {
        set(EBoostBrakeState::None);
    }
}
void ASpaceShip::start_brake() {
    if (energy_is_full() && (boost_brake_state == EBoostBrakeState::None)) {
        set(EBoostBrakeState::Brake);
    }
}
void ASpaceShip::stop_brake() {
    if (boost_brake_state == EBoostBrakeState::Brake) {
        set(EBoostBrakeState::None);
    }
}
void ASpaceShip::roll(float direction) {
#if WITH_EDITOR
    if (can_log()) {
        UE_LOG(LogSandboxActor, Verbose, TEXT("Rolling: %.2f"), direction);
    }
#endif
    manual_bank_direction = clamp(direction, 1.f);
}
void ASpaceShip::barrel_roll(float direction) {
#if WITH_EDITOR
    if (can_log()) {
        UE_LOG(LogSandboxActor, Verbose, TEXT("Barrel rolling: %.2f"), direction);
    }
#endif
}

void ASpaceShip::fire_laser() {
    auto const left{ship_mesh->GetSocketTransform(Sockets::left, RTS_World)};
    auto const right{ship_mesh->GetSocketTransform(Sockets::right, RTS_World)};
    auto const middle{ship_mesh->GetSocketTransform(Sockets::middle, RTS_World)};

    switch (laser_mode) {
        case EShipLaserMode::Single: {
            fire_laser_from(*laser_config, middle);
            break;
        }
        case EShipLaserMode::Double: {
            fire_laser_from(*laser_config, left);
            fire_laser_from(*laser_config, right);
            break;
        }
        case EShipLaserMode::Hyper: {
            fire_laser_from(*hyper_laser_config, left);
            fire_laser_from(*hyper_laser_config, right);
            break;
        }
        default: {
            UE_LOG(LogSandboxActor, Warning, TEXT("Unhandled fire_laser branch."));
            break;
        }
    }
}
void ASpaceShip::fire_laser_from(UShipLaserConfig const& fire_laser_config, FTransform fire_point) {
    TRY_INIT_PTR(world, GetWorld());

    UE_LOG(LogSandboxActor,
           Verbose,
           TEXT("Spawning laser at %s"),
           *fire_point.ToHumanReadableString());

    TRY_INIT_PTR(laser,
                 world->SpawnActorDeferred<AShipLaser>(laser_class, fire_point, nullptr, this));
    laser->set_config(fire_laser_config);
    laser->set_speed(laser_speed);
    laser->FinishSpawning(fire_point);
}
void ASpaceShip::fire_bomb() {
    if (auto ab{active_bomb.Pin()}) {
        if (!ab->has_detonated()) {
            active_bomb.Get()->detonate();
            return;
        }
    }

    if (bombs <= 0) {
        UE_LOG(LogSandboxActor, Verbose, TEXT("No bombs left."));
        return;
    }

    TRY_INIT_PTR(world, GetWorld());
    auto const fire_point{ship_mesh->GetSocketTransform(Sockets::middle, RTS_World)};

    UE_LOG(
        LogSandboxActor, Verbose, TEXT("Spawning bomb at %s"), *fire_point.ToHumanReadableString());

    active_bomb = world->SpawnActorDeferred<AShipBomb>(bomb_class, fire_point, nullptr, this);
    active_bomb->FinishSpawning(fire_point);

    subtract_bomb();
}
void ASpaceShip::upgrade_laser() {
    if (laser_mode == EShipLaserMode::Single) {
        laser_mode = EShipLaserMode::Double;
    } else if (laser_mode == EShipLaserMode::Double) {
        laser_mode = EShipLaserMode::Hyper;
    }
}
void ASpaceShip::add_bomb() {
    bombs++;
    on_bombs_changed.Execute(bombs);
}
void ASpaceShip::subtract_bomb() {
    bombs--;
    on_bombs_changed.Execute(bombs);
}

void ASpaceShip::add_health(int32 added_health) {
    health->add_health(added_health);
}
void ASpaceShip::upgrade_max_health() {
    health->upgrade_max_health();
}
void ASpaceShip::add_gold_ring() {
    gold_rings_collected++;
    if (gold_rings_collected >= 3) {
        upgrade_max_health();
        gold_rings_collected = 0;
    }
    on_gold_rings_changed.Execute(gold_rings_collected);
}
void ASpaceShip::add_points(int32 x) {
    points += x;
    on_points_changed.Execute(points);
}
void ASpaceShip::record_kills(int32 kills) {
    add_points(kills);
}
auto ASpaceShip::get_on_health_changed_delegate() -> FOnShipHealthChanged& {
    return health->on_health_changed;
}

auto ASpaceShip::apply_damage(int32 damage, AActor const& instigator) -> FShipDamageResult {
    auto const original_health{health->get_health()};
    EDamageResult type{EDamageResult::NoEffect};

    health->apply_damage(damage);

    if (health->is_dead()) {
        type = EDamageResult::Killed;
    } else if (health->get_health() < original_health) {
        type = EDamageResult::Damaged;
    } else {
        type = EDamageResult::NoEffect;
    }

    FShipDamageResult result{type};
    return result;
}
void ASpaceShip::add_life() {
    lives += 1;
    on_lives_changed.Execute(lives);
}

void ASpaceShip::sample_speed() {
    speed_samples[speed_sample_index] = {FMath::Clamp(GetWorld()->GetTimeSeconds(), 0.0, 1e9),
                                         FMath::Clamp(velocity.Size(), 0.0, 100e3)};
    speed_sample_index++;
    if (speed_sample_index >= speed_sample_max) {
        speed_sample_index = 0;
    }

    on_speed_sampled.ExecuteIfBound(std::span(speed_samples.GetData(), speed_samples.Num()),
                                    speed_sample_index);
}
