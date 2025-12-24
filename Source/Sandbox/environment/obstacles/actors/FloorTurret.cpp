#include "Sandbox/environment/obstacles/actors/FloorTurret.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Sandbox/combat/projectiles/actors/BulletActor.h"
#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/enums/TeamID.h"
#include "Sandbox/utilities/geometry.h"

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
            handle_attacking_state(dt);
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

    switch (state.operating_state) {
        case EFloorTurretState::Attacking: {
            state.time_since_last_shot = 0.0f;
            break;
        }
        default: {
            break;
        }
    }
}

bool AFloorTurret::within_vision_cone(AActor& target, float cone_degrees) const {
    auto const fwd{camera_mesh->GetForwardVector()};
    auto const camera_location{camera_mesh->GetComponentLocation()};
    auto const to_target{target.GetActorLocation() - camera_location};
    auto const to_target_norm{to_target.GetSafeNormal()};

    auto const angle_between{
        FMath::RadiansToDegrees(ml::get_angle_between_lines(fwd, to_target_norm))};

    return angle_between <= cone_degrees;
}
bool AFloorTurret::is_enemy(AActor& target) const {
    return get_team_attitude(state.team_id, target) == ETeamAttitude::Hostile;
}

void AFloorTurret::BeginPlay() {
    Super::BeginPlay();
}

auto AFloorTurret::search_for_enemy(float vision_radius, float vision_angle) const -> AActor* {
    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), nullptr);

    TArray<FHitResult> hits;
    float const capsule_half_height{vision_radius}; // placeholder
    auto const vision_pos{camera_mesh->GetComponentLocation()};

    auto const capsule{FCollisionShape::MakeCapsule(vision_radius, capsule_half_height)};
    FCollisionQueryParams params;
    params.AddIgnoredActor(Owner);
    params.AddIgnoredActor(this);
    auto const start{vision_pos};
    auto const end{vision_pos};

    auto sweep_hit{
        world->SweepMultiByChannel(hits, start, end, FQuat::Identity, ECC_Pawn, capsule, params)};

    DrawDebugCapsule(world,
                     /*centre*/ vision_pos,
                     /*half height*/ capsule_half_height,
                     /*radius*/ vision_radius,
                     /*rotation*/ FQuat::Identity,
                     /*color*/ FColor::Green);

    constexpr int32 cone_sides{8};
    DrawDebugCone(world,
                  vision_pos,
                  camera_mesh->GetForwardVector(),
                  vision_radius,
                  FMath::DegreesToRadians(vision_angle),
                  FMath::DegreesToRadians(vision_angle),
                  cone_sides,
                  FColor::Blue);

    if (!sweep_hit) {
        return nullptr;
    }

    for (auto const& hit : hits) {
        auto const target{hit.GetActor()};
        if (!target) {
            continue;
        }

        if (!within_vision_cone(*target, vision_angle)) {
            continue;
        }

        if (!is_enemy(*target)) {
            continue;
        }

        auto* health{target->FindComponentByClass<UHealthComponent>()};
        if (!health) {
            continue;
        }

        UE_LOG(LogSandboxAI, Verbose, TEXT("Found enemy: %s\n"), *target->GetActorLabel());

        return hit.GetActor();
    }

    return nullptr;
}
void AFloorTurret::turn_towards_enemy(AActor& enemy) {}

void AFloorTurret::handle_watching_state(float dt) {
    auto const delta_rotation{aim_config.watching_rotation_speed_degrees_per_second * dt};

    auto const max_rot{aim_config.rotation_degrees};

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

    if (auto* enemy{
            search_for_enemy(aim_config.watching_cone_radius, aim_config.watching_cone_degrees)}) {
        state.current_target = enemy;
        set_state(EFloorTurretState::Attacking);
    }
}
void AFloorTurret::handle_attacking_state(float dt) {
    auto* enemy{
        search_for_enemy(aim_config.watching_cone_radius, aim_config.tracking_cone_degrees)};

    if (!enemy) {
        set_state(EFloorTurretState::Watching);
        state.current_target = nullptr;
        return;
    }

    turn_towards_enemy(*enemy);

    state.time_since_last_shot += dt;
    auto const seconds_per_bullet{1.0f / bullet_config.fire_rate};

    if (state.time_since_last_shot >= seconds_per_bullet) {
        fire_bullet();
        state.time_since_last_shot = 0.0f;
    }
}
void AFloorTurret::fire_bullet() {
    check(bullet_config.bullet_actor_class);

    RETURN_IF_NULLPTR(bullet_config.bullet_actor_class);
    RETURN_IF_NULLPTR(muzzle_point);
    TRY_INIT_PTR(world, GetWorld());

    auto const spawn_location{muzzle_point->GetComponentLocation()};
    auto const spawn_rotation{muzzle_point->GetComponentRotation()};

    FActorSpawnParameters spawn_params{};
    spawn_params.Owner = this;
    auto* bullet{world->SpawnActor<ABulletActor>(
        bullet_config.bullet_actor_class, spawn_location, spawn_rotation, spawn_params)};

    RETURN_IF_NULLPTR(bullet);

    TRY_INIT_PTR(movement, bullet->FindComponentByClass<UProjectileMovementComponent>());

    bullet->SetActorLocationAndRotation(spawn_location, spawn_rotation);

    auto const speed{bullet_config.bullet_speed};
    movement->InitialSpeed = speed;
    movement->MaxSpeed = speed;

    auto velocity_unit{spawn_rotation.Vector()};
    velocity_unit.Normalize();
    movement->Velocity = velocity_unit * speed;
}
