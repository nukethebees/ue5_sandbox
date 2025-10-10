#include "Sandbox/actor_components/weapons/PawnWeaponComponent.h"

#include "Sandbox/actor_components/weapons/WeaponComponent.h"
#include "Sandbox/actors/weapons/WeaponBase.h"

#include "Sandbox/macros/null_checks.hpp"

bool UPawnWeaponComponent::can_fire() const {
    RETURN_VALUE_IF_NULLPTR(active_weapon, false);
    return active_weapon->can_fire();
}
void UPawnWeaponComponent::start_firing() {
    RETURN_IF_NULLPTR(active_weapon);
    return active_weapon->start_firing();
}
void UPawnWeaponComponent::sustain_firing(float delta_time) {
    RETURN_IF_NULLPTR(active_weapon);
    return active_weapon->sustain_firing(delta_time);
}
void UPawnWeaponComponent::stop_firing() {
    RETURN_IF_NULLPTR(active_weapon);
    return active_weapon->stop_firing();
}

void UPawnWeaponComponent::reload() {
    RETURN_IF_NULLPTR(active_weapon);
    return active_weapon->reload();
}
bool UPawnWeaponComponent::can_reload() const {
    RETURN_VALUE_IF_NULLPTR(active_weapon, false);
    return active_weapon->can_reload();
}

void UPawnWeaponComponent::equip_weapon(AWeaponBase* weapon) {
    RETURN_IF_NULLPTR(weapon);

    log_display(TEXT("Equipping %s"), *weapon->get_name());

    active_weapon = weapon;
}

void UPawnWeaponComponent::unequip_weapon() {
    RETURN_IF_NULLPTR(active_weapon);

    if (!active_weapon->HasAnyFlags(RF_ClassDefaultObject)) {
        active_weapon->Destroy();
    }

    active_weapon = nullptr;
}
