#include "Sandbox/actor_components/weapons/PawnWeaponComponent.h"

#include "Sandbox/actor_components/weapons/WeaponComponent.h"

#include "Sandbox/macros/null_checks.hpp"

UPawnWeaponComponent::UPawnWeaponComponent()
    : weapon_component{CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"), false)} {}

void UPawnWeaponComponent::OnRegister() {
    Super::OnRegister();
}
void UPawnWeaponComponent::OnUnregister() {
    Super::OnUnregister();
}
