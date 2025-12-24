// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/combat/projectiles/actors/BulletActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "Sandbox/core/object_pooling/data/PoolConfig.h"
#include "Sandbox/core/object_pooling/subsystems/ObjectPoolSubsystem.h"
#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/health/subsystems/DamageManagerSubsystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ABulletActor::ABulletActor()
    : collision_component{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"))}
    , mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))}
    , projectile_movement{
          CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"))} {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletActor::ABulletActor"))

    PrimaryActorTick.bCanEverTick = false;

    collision_component->SetBoxExtent(FVector{1.0f, 1.0f, 1.0f});
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    collision_component->SetCollisionResponseToAllChannels(ECR_Block);
    collision_component->SetNotifyRigidBodyCollision(true);
    RootComponent = collision_component;

    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    projectile_movement->InitialSpeed = 1000.0f;
    projectile_movement->MaxSpeed = 1000.0f;
    projectile_movement->bRotationFollowsVelocity = true;
    projectile_movement->bShouldBounce = false;
    projectile_movement->ProjectileGravityScale = 0.0f;
    projectile_movement->UpdatedComponent = RootComponent;
}

void ABulletActor::BeginPlay() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletActor::BeginPlay"))
    static constexpr auto LOG{NestedLogger<"BeginPlay">()};

    SetLifeSpan(120.0f);

    Super::BeginPlay();

    // Bind to collision event
    RETURN_IF_NULLPTR(collision_component);
    if (collision_component) {
        collision_component->OnComponentHit.AddDynamic(this, &ABulletActor::on_hit);
    }
}

void ABulletActor::on_hit(UPrimitiveComponent* HitComponent,
                          AActor* other_actor,
                          UPrimitiveComponent* other_component,
                          FVector NormalImpulse,
                          FHitResult const& Hit) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletActor::on_hit"))
    static constexpr auto LOG{NestedLogger<"on_hit">()};

    log_very_verbose(TEXT("Hit occurred."));
    TRY_INIT_PTR(world, GetWorld());

    if (impact_effect) {
        auto const impact_location{Hit.Location};
        auto const impact_rotation{UKismetMathLibrary::MakeRotFromZ(Hit.Normal)};

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            world, impact_effect, impact_location, impact_rotation);
    }

    if (other_actor) {
        if (auto* actor_health{other_actor->FindComponentByClass<UHealthComponent>()}) {
            if (auto* manager{world->GetSubsystem<UDamageManagerSubsystem>()}) {
                manager->queue_health_change(actor_health, damage);
            }
        }
    } else {
        LOG.log_warning(TEXT("No other_actor"));
    }

    if (auto* pool{world->GetSubsystem<UObjectPoolSubsystem>()}) {
        if (pool->return_item<FBulletPoolConfig>(this) !=
            EObjectPoolSubsystemReturnStatus::Success) {
            Destroy();
        }
    } else {
        Destroy();
    }
}

void ABulletActor::Activate() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletActor::Activate"))
    static constexpr auto LOG{NestedLogger<"Activate">()};

    if (projectile_movement) {
        projectile_movement->Activate();
        projectile_movement->Velocity = FVector::ZeroVector;
    } else {
        LOG.log_warning(TEXT("projectile_movement is nullptr"));
    }

    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
}
void ABulletActor::Deactivate() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletActor::Deactivate"))
    static constexpr auto LOG{NestedLogger<"Deactivate">()};

    if (projectile_movement) {
        projectile_movement->Deactivate();
        projectile_movement->Velocity = FVector::ZeroVector;
    } else {
        LOG.log_warning(TEXT("projectile_movement is nullptr"));
    }

    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorLocation(FVector::ZeroVector);
}
