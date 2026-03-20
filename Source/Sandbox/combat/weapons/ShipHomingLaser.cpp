#include "Sandbox/combat/weapons/ShipHomingLaser.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/DamageableShip.h"
#include "Sandbox/players/ShipScoringSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AShipHomingLaser::AShipHomingLaser()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("laser_mesh"))}
    , collision_box{CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = collision_box;
    collision_box->SetMobility(EComponentMobility::Movable);
    collision_box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_box->SetCollisionResponseToAllChannels(ECR_Block);
    collision_box->SetNotifyRigidBodyCollision(true);
    collision_box->SetCollisionObjectType(ml::collision::projectile);
    collision_box->BodyInstance.bUseCCD = true;

    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AShipHomingLaser::Tick(float dt) {
    Super::Tick(dt);

    if (!IsValid(target)) {
        explode();
        return;
    }

    auto const cur_loc{GetActorLocation()};
    auto const tgt_loc{target->GetActorLocation()};

    auto const direction{(tgt_loc - cur_loc).GetSafeNormal()};
    auto const delta_movement{speed * dt};
    auto const new_loc{cur_loc + direction * delta_movement};

    FHitResult hit;
    SetActorLocation(new_loc, true, &hit);
    if (hit.bBlockingHit) {
        explode();
    }
}
void AShipHomingLaser::BeginPlay() {
    Super::BeginPlay();

    SetLifeSpan(180.0f);
}
void AShipHomingLaser::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
void AShipHomingLaser::explode() {
    UE_LOG(LogSandboxActor, Verbose, TEXT("AShipHomingLaser::explode"));

    auto* world{GetWorld()};
    if (!world) {
        WARN_IS_FALSE(LogSandboxActor, world);
        Destroy();
        return;
    }

    if (explosion_effect) {
        auto* sys{UNiagaraFunctionLibrary::SpawnSystemAtLocation(world,
                                                                 explosion_effect,
                                                                 GetActorLocation(),
                                                                 GetActorRotation(),
                                                                 FVector::OneVector,
                                                                 true,
                                                                 false)};
        if (sys) {
            sys->SetFloatParameter(TEXT("explosion_radius"), explosion_radius);
            sys->Activate();
        } else {
            WARN_IS_FALSE(LogSandboxActor, explosion_effect);
        }
    } else {
        WARN_IS_FALSE(LogSandboxActor, explosion_effect);
    }

#if WITH_EDITOR
    if (debug_explosion) {
        draw_debug_sphere();
    }
#endif

    apply_damage(*world);

    Destroy();
}

void AShipHomingLaser::apply_damage(UWorld& world) {
    TRY_INIT_PTR(instigator, GetInstigator());

    TArray<FHitResult> hits;
    FCollisionQueryParams params;
    auto const loc{GetActorLocation()};

    world.SweepMultiByChannel(hits,
                              loc,
                              loc,
                              FQuat::Identity,
                              ECC_Pawn,
                              FCollisionShape::MakeSphere(explosion_radius),
                              params);

    for (auto& hit : hits) {
        auto* actor_hit{hit.GetActor()};

        auto* ship{Cast<IDamageableShip>(actor_hit)};
        if (!ship) {
            continue;
        }

        auto const damage_result{ship->apply_damage(damage, *instigator)};
        if (damage_result.was_killed()) {
            TRY_INIT_PTR(ss, world.GetSubsystem<UShipScoringSubsystem>());
            FShipAttackResult kill_result{instigator,
                                          EShipProjectileType::homing_laser,
                                          FShipAttackResult::Actors{actor_hit}};
            ss->register_kills(kill_result);
        }
    }
}

#if WITH_EDITOR
void AShipHomingLaser::draw_debug_sphere() {
    TRY_INIT_PTR(world, GetWorld());
    auto const location{GetActorLocation()};

    constexpr bool persistent_lines{false};
    constexpr int32 segments{16};
    constexpr uint8 depth_priority{0};

    DrawDebugSphere(world,
                    location,
                    explosion_radius,
                    segments,
                    debug_line_colour,
                    persistent_lines,
                    debug_explosion_lifetime,
                    depth_priority,
                    debug_line_thickness);
}
#endif
