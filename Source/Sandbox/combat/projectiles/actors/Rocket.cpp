#include "Sandbox/combat/projectiles/actors/Rocket.h"

#include "GameFramework/ProjectileMovementComponent.h"

ARocket::ARocket()
    : projectile_movement{
          CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"))} {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    this->ammo_type = EAmmoType::Rockets;
    this->quantity = 1;

    projectile_movement->bAutoActivate = false; 
    projectile_movement->InitialSpeed = 0;
    projectile_movement->bRotationFollowsVelocity = true;
    projectile_movement->bShouldBounce = false;
    projectile_movement->ProjectileGravityScale = 0.0f;
}

void ARocket::Tick(float dt) {
    Super::Tick(dt);
}
void ARocket::BeginPlay() {
    Super::BeginPlay();
    SetLifeSpan(120.0f);
}
