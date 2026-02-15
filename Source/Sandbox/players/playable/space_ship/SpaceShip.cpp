#include "Sandbox/players/playable/space_ship/SpaceShip.h"

#include "Sandbox/combat/weapons/ShipBomb.h"
#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/health/ShipHealthComponent.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ASpaceShip::ASpaceShip()
    : camera(CreateDefaultSubobject<UCameraComponent>(TEXT("camera")))
    , spring_arm(CreateDefaultSubobject<USpringArmComponent>(TEXT("spring_arm")))
    , ship_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ship_mesh")))
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box")))
    , health(CreateDefaultSubobject<UShipHealthComponent>(TEXT("health"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    camera->SetupAttachment(spring_arm);
    spring_arm->SetupAttachment(RootComponent);
    ship_mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);

    // Don't tick until the controller wires up the delegates
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}
void ASpaceShip::Tick(float dt) {
    Super::Tick(dt);

#if WITH_EDITOR
    seconds_since_last_log += dt;
#endif

    update_boost_brake(dt);
    update_actor_rotation(dt);
    update_visual_orientation(dt);
    integrate_velocity(dt);

#if WITH_EDITOR
    if (can_log()) {
        seconds_since_last_log = 0.f;
    }
#endif
}
void ASpaceShip::update_boost_brake(this auto& self, float dt) {
    auto const starting_thrust_energy{self.thrust_energy};

    if (starting_thrust_energy <= 0.f) {
        self.boost_brake_state = EBoostBrakeState::None;
    }

    switch (self.boost_brake_state) {
        case EBoostBrakeState::None: {
            self.target_speed = self.cruise_speed;
            self.thrust_energy = starting_thrust_energy + dt * self.thrust_recharge_rate;
            break;
        }
        case EBoostBrakeState::Boost: {
            self.target_speed = self.boost_speed;
            self.thrust_energy = starting_thrust_energy - dt * self.boost_depletion_rate;
            break;
        }
        case EBoostBrakeState::Brake: {
            self.target_speed = self.brake_speed;
            self.thrust_energy = starting_thrust_energy - dt * self.brake_depletion_rate;
            break;
        }
        default: {
            UE_LOG(LogSandboxActor, Error, TEXT("Unhandled state."));
            self.target_speed = self.cruise_speed;
            self.boost_brake_state = EBoostBrakeState::None;
            break;
        }
    }

    self.thrust_energy = FMath::Clamp(self.thrust_energy, 0.f, self.thrust_energy_max);

    if (starting_thrust_energy != self.thrust_energy) {
        self.on_energy_changed.Execute(self.thrust_energy / self.thrust_energy_max);
    }
}
void ASpaceShip::update_actor_rotation(this auto& self, float dt) {
    auto const drot{self.rotation_speed * dt};
    if (self.rotation_input == FVector2D::ZeroVector) {
        auto const rot{self.GetActorRotation()};
        FRotator const delta_rotation{
            self.tick_clamp(-rot.Pitch, dt, drot), 0.0f, self.tick_clamp(-rot.Roll, dt, drot)};

        self.AddActorLocalRotation(delta_rotation);
    } else {
        FRotator const delta_rotation(
            self.rotation_input.Y * drot, self.rotation_input.X * drot, 0.0f);
        self.AddActorLocalRotation(delta_rotation);
    }
}
void ASpaceShip::update_visual_orientation(this auto& self, float dt) {
    auto const current_rotation{self.ship_mesh->GetRelativeRotation()};
    auto const target_pitch{self.rotation_input.Y * self.pitch_angle_max};
    auto const new_pitch{
        FMath::FInterpTo(current_rotation.Pitch, target_pitch, dt, self.pitch_speed)};
    auto const target_roll{self.rotation_input.X * self.bank_angle_max};
    auto const new_roll{FMath::FInterpTo(current_rotation.Roll, target_roll, dt, self.bank_speed)};
    self.ship_mesh->SetRelativeRotation(FRotator(new_pitch, current_rotation.Yaw, new_roll));
}
void ASpaceShip::integrate_velocity(this auto& self, float dt) {
    auto const current_speed{self.velocity.Size()};
    auto const target_speed_delta{self.target_speed - current_speed};
    auto const max_frame_speed_delta{self.max_acceleration * dt};
    auto const frame_speed_delta{self.tick_clamp(target_speed_delta, dt, max_frame_speed_delta)};
    auto const new_speed{current_speed + frame_speed_delta};

    self.velocity = self.GetActorForwardVector() * new_speed;

    auto const delta_pos{self.velocity * dt};
    self.SetActorLocation(self.GetActorLocation() + delta_pos, true);

    self.max_acceleration = self.cruise_acceleration;
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

    target_speed = cruise_speed;
    max_acceleration = cruise_acceleration;
}

void ASpaceShip::turn(FVector2D direction) {
    UE_LOG(LogSandboxActor, Verbose, TEXT("Turning: %s"), *direction.ToString());

    rotation_input = direction;
}
void ASpaceShip::start_boost() {
    if (energy_is_full() && (boost_brake_state == EBoostBrakeState::None)) {
        boost_brake_state = EBoostBrakeState::Boost;
    }
}
void ASpaceShip::stop_boost() {
    if (boost_brake_state == EBoostBrakeState::Boost) {
        boost_brake_state = EBoostBrakeState::None;
    }
}
void ASpaceShip::start_brake() {
    if (energy_is_full() && (boost_brake_state == EBoostBrakeState::None)) {
        boost_brake_state = EBoostBrakeState::Brake;
    }
}
void ASpaceShip::stop_brake() {
    if (boost_brake_state == EBoostBrakeState::Brake) {
        boost_brake_state = EBoostBrakeState::None;
    }
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
    if (active_bomb.IsValid()) {
        active_bomb.Get()->detonate();
        return;
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
auto ASpaceShip::record_kills(EShipProjectileType projectile_type, int32 kills) {
    auto const added_points{projectile_type == EShipProjectileType::homing_laser ? (2 * kills - 1)
                                                                                 : kills};
    add_points(added_points);
}
auto ASpaceShip::get_on_health_changed_delegate() -> FOnShipHealthChanged& {
    return health->on_health_changed;
}
