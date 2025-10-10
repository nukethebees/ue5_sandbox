#include "Sandbox/actors/weapons/WeaponBase.h"

void AWeaponBase::hide_weapon() {
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

void AWeaponBase::show_weapon() {
    SetActorHiddenInGame(false);
}
