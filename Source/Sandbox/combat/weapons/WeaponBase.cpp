#include "Sandbox/combat/weapons/WeaponBase.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Sandbox/interaction/TriggerSubsystem.h"

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

FAmmoReloadResult AWeaponBase::reload(FAmmoData const& offered) {
    FAmmoReloadResult result;
    result.AmmoOfferedRemaining = offered;

    if (offered.type != get_ammo_type()) {
        return result;
    }

    if (!can_reload()) {
        return result;
    }

    auto const needed{get_ammo_needed_for_reload()};
    auto const taken{FMath::Min(needed.amount, offered.amount)};

    ammo += taken;

    result.AmmoTaken = FAmmoData(EAmmoType::Bullets, taken);
    result.AmmoOfferedRemaining.amount -= taken;

    on_ammo_changed.Broadcast(get_current_ammo());

    return result;
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
