#include "Sandbox/actors/weapons/WeaponPickup.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/actor_components/RotatingActorComponent.h"
#include "Sandbox/actors/weapons/WeaponBase.h"

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

    constexpr auto logger{NestedLogger<"BeginPlay">()};

    RETURN_IF_NULLPTR(collision_component);
    collision_component->OnComponentBeginOverlap.AddDynamic(this, &AWeaponPickup::on_overlap_begin);

    RETURN_IF_NULLPTR(weapon_class);
    TRY_INIT_PTR(weapon_cdo, weapon_class->GetDefaultObject<AWeaponBase>());
    TRY_INIT_PTR(display_mesh, weapon_cdo->get_display_mesh());

    weapon_mesh->SetStaticMesh(display_mesh);
}

void AWeaponPickup::on_overlap_begin(UPrimitiveComponent* overlapped_component,
                                     AActor* other_actor,
                                     UPrimitiveComponent* other_component,
                                     int32 other_body_index,
                                     bool from_sweep,
                                     FHitResult const& sweep_result) {
    constexpr auto logger{NestedLogger<"on_overlap_begin">()};

    logger.log_display(TEXT("picking up weapon"));
}
