#include "Sandbox/combat/weapons/actors/WeaponBase.h"

#include "Components/BoxComponent.h"
#include "Sandbox/interaction/triggering/subsystems/TriggerSubsystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AWeaponBase::AWeaponBase()
    : collision_box{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    collision_box->SetupAttachment(RootComponent);
}

void AWeaponBase::set_pickup_collision(bool enabled) {
    RETURN_IF_NULLPTR(collision_box);

    collision_box->SetCollisionEnabled(enabled ? ECollisionEnabled::QueryOnly
                                               : ECollisionEnabled::NoCollision);
    collision_box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AWeaponBase::hide_weapon() {
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}
void AWeaponBase::show_weapon() {
    SetActorHiddenInGame(false);
}

void AWeaponBase::BeginPlay() {
    Super::BeginPlay();

    set_pickup_collision(true);
    trigger_payload.weapon = this;

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(subsystem, world->GetSubsystem<UTriggerSubsystem>());

    subsystem->register_triggerable(*this, std::move(trigger_payload));
}
void AWeaponBase::EndPlay(EEndPlayReason::Type reason) {
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(subsystem, world->GetSubsystem<UTriggerSubsystem>());
    subsystem->deregister_triggerable(*this);

    Super::EndPlay(reason);
}
