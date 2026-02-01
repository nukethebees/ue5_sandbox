#include "Sandbox/environment/obstacles/actors/ExplosiveProp.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

#include "Sandbox/combat/explosion/ExplosionSubsystem.h"
#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/health/actor_components/HealthComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AExplosiveProp::AExplosiveProp()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))}
    , health_component{CreateDefaultSubobject<UHealthComponent>(TEXT("Health"))} {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    // Create mesh component
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    // Allow destruction from bullets
    mesh_component->SetCollisionResponseToChannel(ml::collision::projectile, ECR_Block);
}

void AExplosiveProp::BeginPlay() {
    Super::BeginPlay();
}

void AExplosiveProp::handle_death() {
    constexpr auto logger{NestedLogger<"handle_death">()};

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(explosion_subsystem, world->GetSubsystem<UExplosionSubsystem>());

    auto const actor_location{GetActorLocation()};
    explosion_subsystem->spawn_explosion(actor_location, FRotator::ZeroRotator, explosion_config);

    logger.log_verbose(TEXT("ExplosiveProp exploded at location: %s"), *actor_location.ToString());

    Destroy();
}
