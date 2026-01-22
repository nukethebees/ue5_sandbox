#include "Sandbox/combat/projectiles/actors/Rocket.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Sandbox/constants/collision_channels.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ARocket::ARocket()
    : projectile_movement{
          CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"))} {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    this->ammo_type = EAmmoType::Rockets;
    this->quantity = 1;

    check(collision_box);

    collision_box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_box->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_box->SetCollisionResponseToChannel(ml::collision::interaction, ECR_Block);
    collision_box->SetNotifyRigidBodyCollision(false);

    RootComponent = collision_box;

    mesh_component->SetupAttachment(collision_box);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    projectile_movement->bAutoActivate = false;
    projectile_movement->InitialSpeed = 0;
    projectile_movement->bRotationFollowsVelocity = true;
    projectile_movement->bShouldBounce = false;
    projectile_movement->ProjectileGravityScale = 0.0f;
    projectile_movement->UpdatedComponent = collision_box;
}

void ARocket::Tick(float dt) {
    Super::Tick(dt);
}
void ARocket::fire(float speed) {
    check(projectile_movement);

    projectile_movement->InitialSpeed = speed;
    projectile_movement->MaxSpeed = speed;
    projectile_movement->Velocity = GetActorForwardVector() * speed;

    projectile_movement->Activate();

    RETURN_IF_NULLPTR(collision_box);
    collision_box->OnComponentHit.AddDynamic(this, &ThisClass::on_hit);

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

void ARocket::BeginPlay() {
    Super::BeginPlay();
    SetLifeSpan(120.0f);
}

void ARocket::on_hit(UPrimitiveComponent* HitComponent,
                     AActor* other_actor,
                     UPrimitiveComponent* other_component,
                     FVector NormalImpulse,
                     FHitResult const& Hit) {
    UE_LOG(LogSandboxCore, Warning, TEXT("Foo"));
}
