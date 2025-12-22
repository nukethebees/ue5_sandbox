#include "Sandbox/environment/obstacles/actors/FloorTurret.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AFloorTurret::AFloorTurret()
    : base_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBase"))}
    , pivot{CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"))}
    , camera_pivot{CreateDefaultSubobject<USceneComponent>(TEXT("TurretCameraPivot"))}
    , cannon_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretCannon"))}
    , camera_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretCamera"))}
    , muzzle_point{CreateDefaultSubobject<UArrowComponent>(TEXT("TurretMuzzlePoint"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    base_mesh->SetupAttachment(RootComponent);
    pivot->SetupAttachment(base_mesh);
    cannon_mesh->SetupAttachment(pivot);
    muzzle_point->SetupAttachment(cannon_mesh);

    camera_pivot->SetupAttachment(pivot);
    camera_mesh->SetupAttachment(camera_pivot);
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

    auto const max_rot{aim_limits.rotation_degrees};

    if (aim_state.scan_direction == EFloorTurretScanDirection::clockwise) {
        aim_state.camera_rotation_angle =
            std::min(aim_state.camera_rotation_angle + delta_rotation, max_rot);
        if (FMath::IsNearlyEqual(aim_state.camera_rotation_angle, max_rot)) {
            aim_state.scan_direction = EFloorTurretScanDirection::anticlockwise;
        }
    } else {
        aim_state.camera_rotation_angle =
            std::max(aim_state.camera_rotation_angle - delta_rotation, -max_rot);
        if (FMath::IsNearlyEqual(aim_state.camera_rotation_angle, -max_rot)) {
            aim_state.scan_direction = EFloorTurretScanDirection::clockwise;
        }
    }

    auto cannon_rotation{pivot->GetRelativeRotation()};
    cannon_rotation.Yaw = aim_state.camera_rotation_angle;
    pivot->SetRelativeRotation(cannon_rotation);

    // Perform raycast
    TRY_INIT_PTR(world, GetWorld());

    TArray<FHitResult> hits;
    float const capsule_half_height{aim_limits.watching_cone_radius}; // placeholder
    auto const vision_pos{camera_mesh->GetComponentLocation()};
    auto const vision_angle{aim_limits.watching_cone_degrees};

    auto const capsule{
        FCollisionShape::MakeCapsule(aim_limits.watching_cone_radius, capsule_half_height)};
    FCollisionQueryParams params;
    auto const start{vision_pos};
    auto const end{vision_pos};

    auto sweep_hit{
        world->SweepMultiByChannel(hits, start, end, FQuat::Identity, ECC_Pawn, capsule, params)};

    if (!sweep_hit) {
        return;
    }

    for (auto const& hit : hits) {
        auto const target{hit.GetActor()};
        if (!target) {
            continue;
        }

        if (!within_vision_cone(*target, aim_limits.watching_cone_degrees)) {
            continue;
        }

        if (!is_enemy(*target)) {
            continue;
        }

        state.current_target = hit.GetActor();
        state.operating_state = EFloorTurretState::Attacking;
        break;
    }

    {
        DrawDebugCapsule(world,
                         /*centre*/ vision_pos,
                         /*half height*/ capsule_half_height,
                         /*radius*/ aim_limits.watching_cone_radius,
                         /*rotation*/ FQuat::Identity,
                         /*color*/ FColor::Green);

        constexpr int32 cone_sides{8};
        DrawDebugCone(world,
                      vision_pos,
                      camera_mesh->GetForwardVector(),
                      aim_limits.watching_cone_radius,
                      FMath::DegreesToRadians(vision_angle),
                      FMath::DegreesToRadians(vision_angle),
                      cone_sides,
                      FColor::Blue);
    }
}
void AFloorTurret::handle_attacking_state() {}
bool AFloorTurret::within_vision_cone(AActor& target, float cone_degrees) const {
    return false;
}
bool AFloorTurret::is_enemy(AActor& target) const {
    return false;
}
