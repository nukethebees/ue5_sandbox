#include "Sandbox/combat/weapons/ShipLaser.h"

#include "Sandbox/combat/weapons/ShipLaserConfig.h"
#include "Sandbox/health/actor_components/HealthComponent.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstance.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AShipLaser::AShipLaser()
    : collision_component{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"))}
    , mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    collision_component->SetupAttachment(RootComponent);
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    collision_component->SetCollisionResponseToAllChannels(ECR_Block);
    collision_component->SetNotifyRigidBodyCollision(true);

    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AShipLaser::Tick(float dt) {
    Super::Tick(dt);

    auto const velocity{GetActorForwardVector() * speed};

    SetActorLocation(GetActorLocation() + velocity * dt);
}
void AShipLaser::set_config(UShipLaserConfig const& config) {
    material = config.material;
    set_speed(config.speed);
    damage.value = config.damage;
}
void AShipLaser::BeginPlay() {
    Super::BeginPlay();

    SetLifeSpan(20.0f);

    RETURN_IF_NULLPTR(collision_component);
    if (collision_component) {
        collision_component->OnComponentHit.AddDynamic(this, &AShipLaser::on_hit);
    }
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
    Destroy();
}
