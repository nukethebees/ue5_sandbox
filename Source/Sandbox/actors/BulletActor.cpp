// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actors/BulletActor.h"

#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/subsystems/DamageManagerSubsystem.h"

ABulletActor::ABulletActor() {
    PrimaryActorTick.bCanEverTick = false;

    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
    RootComponent = collision_component;
    collision_component->SetBoxExtent(FVector{1.0f, 1.0f, 1.0f});
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    collision_component->SetCollisionResponseToAllChannels(ECR_Block);
    collision_component->SetNotifyRigidBodyCollision(true);

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
        log_warning(TEXT("No OtherActor"));
    }

    Destroy();
}
