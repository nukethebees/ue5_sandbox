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
    auto const can_log{seconds_since_last_log >= seconds_per_log};
#endif

    constexpr auto clamp{
        [](auto x, auto dt, auto abs_max) { return FMath::Clamp(x * dt, -abs_max, abs_max); }};

    auto const drot{rotation_speed * dt};
    if (rotation_input == FVector2D::ZeroVector) {
        auto const rot{GetActorRotation()};
        FRotator const delta_rotation{
            clamp(-rot.Pitch, dt, drot), 0.0f, clamp(-rot.Roll, dt, drot)};

        AddActorLocalRotation(delta_rotation);
#if WITH_EDITOR
        if (can_log) {
            UE_LOG(LogSandboxActor, Verbose, TEXT("Levelling off."));
            UE_LOG(LogSandboxActor, Verbose, TEXT("Cur rot: %s"), *rot.ToCompactString());
            UE_LOG(LogSandboxActor,
                   Verbose,
                   TEXT("delta Rotation: %s"),
                   *delta_rotation.ToCompactString());
        }
#endif
    } else {
        FRotator const delta_rotation(rotation_input.Y * drot, rotation_input.X * drot, 0.0f);
        AddActorLocalRotation(delta_rotation);

#if WITH_EDITOR
        if (can_log) {
            UE_LOG(LogSandboxActor, Verbose, TEXT("Turning."));
            UE_LOG(LogSandboxActor, Verbose, TEXT("Rotation: %s"), *delta_rotation.ToString());
        }
#endif
    }

    auto const current_rotation{ship_mesh->GetRelativeRotation()};

    auto const target_pitch{rotation_input.Y * pitch_angle_max};
    auto const new_pitch{FMath::FInterpTo(current_rotation.Pitch, target_pitch, dt, pitch_speed)};
    auto const target_roll{rotation_input.X * bank_angle_max};
    auto const new_roll{FMath::FInterpTo(current_rotation.Roll, target_roll, dt, bank_speed)};
    ship_mesh->SetRelativeRotation(FRotator(new_pitch, current_rotation.Yaw, new_roll));

    auto const current_speed{velocity.Size()};
    auto const target_speed_delta{target_speed - current_speed};
    auto const max_frame_speed_delta{max_acceleration * dt};
    auto const frame_speed_delta{clamp(target_speed_delta, dt, max_frame_speed_delta)};
    auto const new_speed{current_speed + frame_speed_delta};

    velocity = GetActorForwardVector() * new_speed;

    auto const delta_pos{velocity * dt};
    SetActorLocation(GetActorLocation() + delta_pos, true);

#if WITH_EDITOR
    if (can_log) {
        seconds_since_last_log = 0.f;
    }
#endif

    target_speed = cruise_speed;
    max_acceleration = cruise_acceleration;
    on_speed_changed.Execute(new_speed);
}

void ASpaceShip::BeginPlay() {
    Super::BeginPlay();

    velocity = GetActorForwardVector() * cruise_speed;

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
void ASpaceShip::boost() {
    target_speed = boost_speed;
    max_acceleration = boost_acceleration;
}
void ASpaceShip::brake() {
    target_speed = brake_speed;
    max_acceleration = brake_acceleration;
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

void ASpaceShip::upgrade_health() {}
void ASpaceShip::add_gold_ring() {
    gold_rings_collected++;
    if (gold_rings_collected >= 3) {
        upgrade_health();
        gold_rings_collected = 0;
    }
    on_gold_rings_changed.Execute(gold_rings_collected);
}
void ASpaceShip::add_points(int32 x) {
    points += x;
    on_points_changed.Execute(points);
}
auto ASpaceShip::get_on_health_changed_delegate() -> FOnShipHealthChanged& {
    return health->on_health_changed;
}
