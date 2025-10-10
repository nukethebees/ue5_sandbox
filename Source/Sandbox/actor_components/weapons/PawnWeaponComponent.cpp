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
    TRY_INIT_PTR(world, GetWorld());

    unequip_weapon();

    log_display(TEXT("Equipping %s"), *weapon->get_name());

    FVector const translation{0.0f, 0.0f, 200.0f};
    auto const rotation{FRotator{}};
    auto const scale{FVector::OneVector};
    FTransform transform{rotation, translation, scale};

    FActorSpawnParameters spawn_parameters{};
    spawn_parameters.Name = TEXT("SpawnedGun");
    spawn_parameters.Owner = GetOwner();

    TRY_INIT_PTR(spawned_weapon,
                 world->SpawnActor(weapon->GetClass(), &transform, spawn_parameters));

    active_weapon = Cast<AWeaponBase>(spawned_weapon);
    RETURN_IF_NULLPTR(active_weapon);
}

void UPawnWeaponComponent::unequip_weapon() {
    if (!active_weapon) {
        return;
    }

    if (!active_weapon->HasAnyFlags(RF_ClassDefaultObject)) {
        active_weapon->Destroy();
    }

    active_weapon = nullptr;
}
