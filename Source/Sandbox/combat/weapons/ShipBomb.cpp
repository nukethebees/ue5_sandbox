#include "Sandbox/combat/weapons/ShipBomb.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/DamageableShip.h"
#include "Sandbox/players/playable/space_ship/ShipScoringSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AShipBomb::AShipBomb()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AShipBomb::Tick(float dt) {
    Super::Tick(dt);

    if (explosion_complete()) {
        Destroy();
    }

    if (!detonated) {
        auto const velocity{GetActorForwardVector() * speed};
        auto const delta_pos{velocity * dt};
        SetActorLocation(GetActorLocation() + delta_pos);
    } else {
        TRY_INIT_PTR(world, GetWorld());
        TRY_INIT_PTR(instigator, GetInstigator());

        TArray<FHitResult> hits;
        FCollisionQueryParams params;
        params.AddIgnoredActor(this);
        params.AddIgnoredActor(instigator);
        auto const loc{GetActorLocation()};

        world->SweepMultiByChannel(hits,
                                   loc,
                                   loc,
                                   FQuat::Identity,
                                   ml::collision::projectile,
                                   FCollisionShape::MakeSphere(explosion_radius),
                                   params);

        for (auto& hit : hits) {
            auto* actor_hit{hit.GetActor()};
            if (actors_hit.Contains(actor_hit)) {
                continue;
            }
            actors_hit.Add(actor_hit);

            auto* ship{Cast<IDamageableShip>(actor_hit)};
            if (!ship) {
                continue;
            }

            auto const damage_result{ship->apply_damage(damage, *instigator)};
            if (damage_result.was_killed()) {
                TRY_INIT_PTR(ss, world->GetSubsystem<UShipScoringSubsystem>());
                FShipAttackResult kill_result{
                    instigator, EShipProjectileType::bomb, FShipAttackResult::Actors{actor_hit}};
                ss->register_kills(kill_result);
            }
        }

        time_since_detonation += dt;
    }
}

void AShipBomb::detonate() {
    UE_LOG(LogSandboxActor, Verbose, TEXT("Bomb detonating."));

    speed = 0.f;
    mesh_component->SetVisibility(false, true);
    detonated = true;

    auto& tm{GetWorldTimerManager()};
    tm.ClearTimer(timer);

    TRY_INIT_PTR(world, GetWorld());
    auto const location{GetActorLocation()};

    constexpr bool persistent_lines{false};
    constexpr int32 segments{16};
    constexpr uint8 depth_priority{0};
    constexpr auto thickness{15.0f};
    DrawDebugSphere(world,
                    location,
                    explosion_radius,
                    segments,
                    FColor::Orange,
                    persistent_lines,
                    explosion_lifetime,
                    depth_priority,
                    thickness);

    time_since_detonation = 0.f;
}

void AShipBomb::BeginPlay() {
    Super::BeginPlay();

    auto& tm{GetWorldTimerManager()};
    tm.SetTimer(timer, this, &ThisClass::detonate, fuse_time, false);
}
