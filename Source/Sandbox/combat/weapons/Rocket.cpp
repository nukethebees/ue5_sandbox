#include "Sandbox/combat/weapons/Rocket.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ARocket::ARocket()
    : projectile_movement{CreateDefaultSubobject<UProjectileMovementComponent>(
          TEXT("ProjectileMovement"))}
    , collision_box{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"))}
    , mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))} {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    check(collision_box);

    RootComponent = collision_box;
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    projectile_movement->bAutoActivate = false;
    projectile_movement->InitialSpeed = 0;
    projectile_movement->bRotationFollowsVelocity = true;
    projectile_movement->bShouldBounce = false;
    projectile_movement->ProjectileGravityScale = 0.0f;
    projectile_movement->UpdatedComponent = collision_box;

    using enum ECollisionChannel;
    using enum ECollisionResponse;

    collision_box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    collision_box->SetNotifyRigidBodyCollision(true);
    collision_box->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    collision_box->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    collision_box->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    collision_box->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
    collision_box->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
    collision_box->SetCollisionResponseToChannel(ml::collision::projectile, ECR_Block);
    collision_box->BodyInstance.bUseCCD = true;
}

void ARocket::Tick(float dt) {
    Super::Tick(dt);
}
void ARocket::fire(FRocketConfig rocket_config) {
    config = rocket_config;

    check(projectile_movement);

    projectile_movement->InitialSpeed = config.speed;
    projectile_movement->MaxSpeed = config.speed;
    projectile_movement->Velocity = GetActorForwardVector() * config.speed;

    projectile_movement->Activate();

    RETURN_IF_NULLPTR(collision_box);
    collision_box->OnComponentHit.AddDynamic(this, &ThisClass::on_hit);
}

void ARocket::BeginPlay() {
    Super::BeginPlay();
    SetLifeSpan(120.0f);

#if WITH_EDITOR
    if (fire_on_launch) {
        fire(config);
    }
#endif
}

void ARocket::on_hit(UPrimitiveComponent* HitComponent,
                     AActor* other_actor,
                     UPrimitiveComponent* other_component,
                     FVector NormalImpulse,
                     FHitResult const& Hit) {
    TRY_INIT_PTR(world, GetWorld());

    // Explosion
    TArray<FOverlapResult> overlaps;
    auto const location{GetActorLocation()};

    auto sphere{FCollisionShape::MakeSphere(config.explosion_radius)};

    FCollisionQueryParams params;

    auto hit{world->OverlapMultiByChannel(
        overlaps, location, FQuat::Identity, ECC_Pawn, sphere, params)};

    for (auto& overlap : overlaps) {
        auto* actor{overlap.GetActor()};
        if (!actor) {
            continue;
        }

        if (auto* hp{actor->GetComponentByClass<UHealthComponent>()}) {
            hp->modify_health(config.damage);
        }
    }

    // Debug visual
    constexpr bool persistent_lines{false};
    constexpr int32 segments{16};
    constexpr auto lifetime{5.0f};
    constexpr uint8 depth_priority{0};
    constexpr auto thickness{2.0f};
    DrawDebugSphere(world,
                    location,
                    config.explosion_radius,
                    segments,
                    FColor::Orange,
                    persistent_lines,
                    lifetime,
                    depth_priority,
                    thickness);

    Destroy();
}
