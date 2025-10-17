#include "Sandbox/actors/weapons/WeaponPickup.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/actor_components/RotatingActorComponent.h"
#include "Sandbox/actor_components/weapons/PawnWeaponComponent.h"
#include "Sandbox/actors/weapons/WeaponBase.h"
#include "Sandbox/subsystems/world/RotationManagerSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

AWeaponPickup::AWeaponPickup()
    : collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent")))
    , rotator(CreateDefaultSubobject<URotatingActorComponent>(TEXT("RotationComponent"))) {

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    collision_box->SetupAttachment(RootComponent);

    rotator->autoregster_parent_root = false;

    PrimaryActorTick.bCanEverTick = false;

    spawn_parameters.Name = TEXT("Weapon");
    spawn_parameters.Owner = this;
    spawn_parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
}
void AWeaponPickup::OnConstruction(FTransform const& Transform) {
    Super::OnConstruction(Transform);

#if WITH_EDITOR
    if (this->bIsEditorPreviewActor) {
        return;
    }
#endif

    TRY_INIT_PTR(world, GetWorld());
    update_weapon(*world);
}
void AWeaponPickup::BeginPlay() {
    Super::BeginPlay();
    constexpr auto logger{NestedLogger<"BeginPlay">()};

    RETURN_IF_NULLPTR(collision_box);
    collision_box->OnComponentBeginOverlap.AddDynamic(this, &AWeaponPickup::on_overlap_begin);

    RETURN_IF_NULLPTR(rotator);
    RETURN_IF_NULLPTR(spawned_weapon);

    rotator->register_scene_component(*spawned_weapon->GetRootComponent());
    spawned_weapon->set_pickup_collision(false);
}
void AWeaponPickup::spawn_weapon(UWorld& world) {
    spawned_weapon = world.SpawnActor<AWeaponBase>(weapon_class, FTransform{}, spawn_parameters);
    RETURN_IF_NULLPTR(spawned_weapon);
    attach_weapon();
}
void AWeaponPickup::update_weapon(UWorld& world) {
    RETURN_IF_NULLPTR(weapon_class);
    RETURN_IF_NULLPTR(weapon_class);

    if (!spawned_weapon || (spawned_weapon->GetClass() != weapon_class)) {
        spawn_weapon(world);
        return;
    }

    attach_weapon();
}
void AWeaponPickup::attach_weapon() {
    constexpr bool weld_to_parent{false};
    spawned_weapon->SetOwner(this);
    spawned_weapon->AttachToActor(
        this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, weld_to_parent));
}
void AWeaponPickup::on_overlap_begin(UPrimitiveComponent* overlapped_component,
                                     AActor* other_actor,
                                     UPrimitiveComponent* other_component,
                                     int32 other_body_index,
                                     bool from_sweep,
                                     FHitResult const& sweep_result) {
    constexpr auto logger{NestedLogger<"on_overlap_begin">()};
    logger.log_verbose(TEXT("Picking up weapon"));

    RETURN_IF_NULLPTR(other_actor);
    RETURN_IF_NULLPTR(spawned_weapon);
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(weapon_component, other_actor->GetComponentByClass<UPawnWeaponComponent>());
    TRY_INIT_PTR(rotation_manager, world->GetSubsystem<URotationManagerSubsystem>());

    spawned_weapon->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
    if (weapon_component->pickup_new_weapon(*spawned_weapon)) {
        rotation_manager->remove(*spawned_weapon->GetRootComponent());
        Destroy();
    } else {
        logger.log_error(TEXT("Failed to pickup weapon"));
    }
}
