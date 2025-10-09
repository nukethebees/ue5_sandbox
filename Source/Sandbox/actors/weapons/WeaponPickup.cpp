#include "Sandbox/actors/weapons/WeaponPickup.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/actor_components/RotatingActorComponent.h"

#include "Sandbox/macros/null_checks.hpp"

AWeaponPickup::AWeaponPickup()
    : weapon_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh")))
    , collision_component(CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent")))
    , rotation_component(
          CreateDefaultSubobject<URotatingActorComponent>(TEXT("RotationComponent"))) {

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    weapon_mesh->SetupAttachment(RootComponent);
    collision_component->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
}

void AWeaponPickup::BeginPlay() {
    Super::BeginPlay();

    RETURN_IF_NULLPTR(collision_component);

    collision_component->OnComponentBeginOverlap.AddDynamic(this, &AWeaponPickup::on_overlap_begin);
}

void AWeaponPickup::on_overlap_begin(UPrimitiveComponent* overlapped_component,
                                     AActor* other_actor,
                                     UPrimitiveComponent* other_component,
                                     int32 other_body_index,
                                     bool from_sweep,
                                     FHitResult const& sweep_result) {}
