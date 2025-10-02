// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actors/BulletActor.h"

#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/subsystems/world/DamageManagerSubsystem.h"
#include "Sandbox/subsystems/world/ObjectPoolSubsystem.h"
#include "Sandbox/utilities/actor_utils.h"

ABulletActor::ABulletActor() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletActor::ABulletActor"))

    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
    collision_component->SetBoxExtent(FVector{1.0f, 1.0f, 1.0f});
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    collision_component->SetCollisionResponseToAllChannels(ECR_Block);
    collision_component->SetNotifyRigidBodyCollision(true);
    collision_component->SetupAttachment(RootComponent);

    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    projectile_movement =
        CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    projectile_movement->InitialSpeed = 1000.0f;
    projectile_movement->MaxSpeed = 1000.0f;
    projectile_movement->bRotationFollowsVelocity = true;
    projectile_movement->bShouldBounce = false;
    projectile_movement->ProjectileGravityScale = 0.0f;
}

void ABulletActor::BeginPlay() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletActor::BeginPlay"))
    static constexpr auto LOG{NestedLogger<"BeginPlay">()};

    Super::BeginPlay();

    // Bind to collision event
    if (collision_component) {
        collision_component->OnComponentHit.AddDynamic(this, &ABulletActor::on_hit);
    }
}

void ABulletActor::on_hit(UPrimitiveComponent* HitComponent,
                          AActor* OtherActor,
                          UPrimitiveComponent* OtherComponent,
                          FVector NormalImpulse,
                          FHitResult const& Hit) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletActor::on_hit"))
    static constexpr auto LOG{NestedLogger<"on_hit">()};

    log_very_verbose(TEXT("Hit occurred."));

    if (impact_effect) {
        auto const impact_location{Hit.Location};
        auto const impact_rotation{UKismetMathLibrary::MakeRotFromZ(Hit.Normal)};

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), impact_effect, impact_location, impact_rotation);
    }

    if (OtherActor) {
        if (auto* actor_health{OtherActor->FindComponentByClass<UHealthComponent>()}) {
            if (auto* manager{GetWorld()->GetSubsystem<UDamageManagerSubsystem>()}) {
                manager->queue_health_change(actor_health, damage);
            }
        }
    } else {
        LOG.log_warning(TEXT("No OtherActor"));
    }

    if (auto* pool{GetWorld()->GetSubsystem<UObjectPoolSubsystem>()}) {
        pool->return_item<FBulletPoolConfig>(this);
    } else {
        LOG.log_warning(TEXT("No object pool subsystem, destroying instead"));
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
