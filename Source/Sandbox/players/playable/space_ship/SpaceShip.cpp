#include "Sandbox/players/playable/space_ship/SpaceShip.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
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
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    camera->SetupAttachment(spring_arm);
    spring_arm->SetupAttachment(RootComponent);
    ship_mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);
}
void ASpaceShip::Tick(float dt) {
    Super::Tick(dt);

#if WITH_EDITOR
    seconds_since_last_log += dt;
    auto const can_log{seconds_since_last_log >= seconds_per_log};
#endif

    auto const drot{rotation_speed * dt};
    if (rotation_input == FVector2D::ZeroVector) {
        constexpr auto clamp{
            [](float rot, float dt, float max) { return FMath::Clamp(rot * dt, -max, max); }};

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

    velocity = GetActorForwardVector() * velocity.Size();

    auto const delta_pos{velocity * dt};
    SetActorLocation(GetActorLocation() + delta_pos, true);

#if WITH_EDITOR
    if (can_log) {
        seconds_since_last_log = 0.f;
    }
#endif
}

void ASpaceShip::BeginPlay() {
    Super::BeginPlay();

    velocity = GetActorForwardVector() * cruise_speed;
}

void ASpaceShip::turn(FVector2D direction) {
    UE_LOG(LogSandboxActor, Verbose, TEXT("Turning: %s"), *direction.ToString());

    rotation_input = direction;
}
void ASpaceShip::fire_laser() {
    check(ship_mesh);
    RETURN_IF_NULLPTR(ship_mesh);
    check(laser_class);
    RETURN_IF_NULLPTR(laser_class);
    check(laser_config);
    RETURN_IF_NULLPTR(laser_config);
    check(hyper_laser_config);
    RETURN_IF_NULLPTR(hyper_laser_config);


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
                 world->SpawnActorDeferred<AShipLaser>(laser_class, fire_point, this, this));
    laser->set_config(fire_laser_config);
    laser->set_speed(laser_speed);
    laser->FinishSpawning(fire_point);
}
void ASpaceShip::fire_bomb() {}
