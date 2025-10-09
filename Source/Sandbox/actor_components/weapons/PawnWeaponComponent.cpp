#include "Sandbox/actor_components/weapons/PawnWeaponComponent.h"

UPawnWeaponComponent::UPawnWeaponComponent() {}

void UPawnWeaponComponent::OnRegister() {
    Super::OnRegister();
    // Register weapon_component with the parent
}
void UPawnWeaponComponent::OnUnregister() {
    Super::OnUnregister();
    // Unregister weapon_component with the parent
}
