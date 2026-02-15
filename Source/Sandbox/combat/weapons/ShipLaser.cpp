#include "Sandbox/combat/weapons/ShipLaser.h"

#include "Sandbox/combat/weapons/ShipLaserConfig.h"
#include "Sandbox/constants/collision_channels.h"
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
    : collision_component{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"))}
    , mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    collision_component->SetupAttachment(RootComponent);
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    collision_component->SetCollisionResponseToAllChannels(ECR_Block);
    collision_component->SetNotifyRigidBodyCollision(true);
    collision_component->SetCollisionObjectType(ml::collision::projectile);
    collision_component->BodyInstance.bUseCCD = true;

    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AShipLaser::Tick(float dt) {
    Super::Tick(dt);

    auto const velocity{GetActorForwardVector() * speed};
    auto const start_location{GetActorLocation()};
    auto const delta_location{velocity * dt};
    auto const end_location{start_location + delta_location};

    SetActorLocation(end_location);

    FHitResult hit;
    FCollisionQueryParams params;
    params.AddIgnoredActor(this);
    params.AddIgnoredActor(GetInstigator());
    auto const box_extent{collision_component->GetScaledBoxExtent()};

    TRY_INIT_PTR(world, GetWorld());

    auto had_hit{world->SweepSingleByChannel(hit,
                                             start_location,
                                             end_location,
                                             FQuat::Identity,
                                             ml::collision::projectile,
                                             FCollisionShape::MakeBox(box_extent),
                                             params)};

    if (had_hit) {
        on_hit(collision_component, hit.GetActor(), hit.GetComponent(), hit.ImpactNormal, hit);
        Destroy();
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
        do_hit(*other_actor);
    }

    Destroy();
}
void AShipLaser::do_hit(AActor& actor) {
    auto* ship{Cast<IDamageableShip>(&actor)};
    if (!ship) {
        return;
    }

    TRY_INIT_PTR(instigator, GetInstigator());
    auto const damage_result{ship->apply_damage(damage, *instigator)};

    if (!damage_result.was_killed()) {
        return;
    }

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(ss, world->GetSubsystem<UShipScoringSubsystem>());

    FShipAttackResult kill_result{
        instigator, EShipProjectileType::laser, FShipAttackResult::Actors{&actor}};

    ss->register_kills(kill_result);
}
