#include "Sandbox/combat/weapons/ShipLaser.h"

#include "Sandbox/combat/weapons/ShipLaserConfig.h"
#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/DamageableShip.h"
#include "Sandbox/players/ShipScoringSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstance.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AShipLaser::AShipLaser()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = mesh_component;
    mesh_component->SetMobility(EComponentMobility::Movable);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    mesh_component->SetCollisionResponseToAllChannels(ECR_Block);
    mesh_component->SetNotifyRigidBodyCollision(true);
    mesh_component->SetCollisionObjectType(ml::collision::projectile);
    mesh_component->BodyInstance.bUseCCD = true;
}

void AShipLaser::Tick(float dt) {
    Super::Tick(dt);

    auto const velocity{GetActorForwardVector() * speed};
    auto const start_location{GetActorLocation()};
    auto const delta_location{velocity * dt};
    auto const end_location{start_location + delta_location};

    FHitResult hit;
    SetActorLocation(end_location, true, &hit);
    if (hit.bBlockingHit) {
        on_hit(mesh_component, hit.GetActor(), hit.GetComponent(), hit.ImpactNormal, hit);
        Destroy();
        return;
    }
}
void AShipLaser::set_config(UShipLaserConfig const& config) {
    material = config.material;
    set_speed(config.speed);
    damage = config.damage;
}
void AShipLaser::BeginPlay() {
    Super::BeginPlay();

    SetLifeSpan(20.0f);
}
void AShipLaser::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    RETURN_IF_NULLPTR(material);
    RETURN_IF_NULLPTR(mesh_component);

    mesh_component->SetMaterial(0, material);
}

void AShipLaser::on_hit(UPrimitiveComponent* HitComponent,
                        AActor* other_actor,
                        UPrimitiveComponent* other_component,
                        FVector NormalImpulse,
                        FHitResult const& Hit) {
    if (other_actor) {
        UE_LOG(LogSandboxWeapon,
               Verbose,
               TEXT("Laser hit: %s"),
               *ml::get_best_display_name(*other_actor));
        do_hit(*other_actor, *HitComponent);
    } else {
        UE_LOG(LogSandboxWeapon, Verbose, TEXT("Actor is null"));
    }

    Destroy();
}
void AShipLaser::do_hit(AActor& actor, UPrimitiveComponent& hit_component) {
    auto* ship{Cast<IDamageableShip>(&actor)};
    if (!ship) {
        return;
    }

    TRY_INIT_PTR(instigator, GetInstigator());
    auto const damage_result{ship->apply_damage({damage, *instigator, hit_component})};

    if (!damage_result.was_killed()) {
        return;
    }

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(ss, world->GetSubsystem<UShipScoringSubsystem>());

    FShipAttackResult kill_result{
        instigator, EShipProjectileType::laser, FShipAttackResult::Actors{&actor}};

    ss->register_kills(kill_result);
}
