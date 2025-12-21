#include "Sandbox/environment/obstacles/actors/FloorTurret.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/logging/SandboxLogCategories.h"

AFloorTurret::AFloorTurret()
    : base_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBase"))}
    , cannon_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretCannon"))}
    , camera_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretCamera"))}
    , muzzle_point{CreateDefaultSubobject<UArrowComponent>(TEXT("TurretMuzzlePoint"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    base_mesh->SetupAttachment(RootComponent);
    cannon_mesh->SetupAttachment(base_mesh);
    muzzle_point->SetupAttachment(cannon_mesh);
    camera_mesh->SetupAttachment(base_mesh);
}

void AFloorTurret::Tick(float dt) {
    Super::Tick(dt);

    switch (state.operating_state) {
        case EFloorTurretState::Disabled: {
            SetActorTickEnabled(false);
            break;
        }
        case EFloorTurretState::Watching: {
            handle_watching_state(dt);
            break;
        }
        case EFloorTurretState::Attacking: {
            handle_attacking_state();
            break;
        }
        default: {
            UE_LOG(LogSandboxActor, Warning, TEXT("Unhandled enum state."));
            break;
        }
    }
}
void AFloorTurret::set_state(EFloorTurretState new_state) {
    state.operating_state = new_state;
    SetActorTickEnabled(state.operating_state != EFloorTurretState::Disabled);
}

void AFloorTurret::BeginPlay() {
    Super::BeginPlay();
}

void AFloorTurret::handle_watching_state(float dt) {
    auto const delta_rotation{aim_limits.rotation_speed_degrees_per_second * dt};

    if (aim_state.scan_direction == EFloorTurretScanDirection::clockwise) {
        aim_state.camera_rotation_angle = std::min(aim_state.camera_rotation_angle + delta_rotation,
                                                   aim_limits.tracking_cone_degrees);
        if (FMath::IsNearlyEqual(aim_state.camera_rotation_angle,
                                 aim_limits.tracking_cone_degrees)) {
            aim_state.scan_direction = EFloorTurretScanDirection::anticlockwise;
        }
    } else {
        aim_state.camera_rotation_angle = std::max(
            -aim_state.camera_rotation_angle - delta_rotation, -aim_limits.tracking_cone_degrees);
        if (FMath::IsNearlyEqual(aim_state.camera_rotation_angle,
                                 aim_limits.tracking_cone_degrees)) {
            aim_state.scan_direction = EFloorTurretScanDirection::clockwise;
        }
    }
}
void AFloorTurret::handle_attacking_state() {}
