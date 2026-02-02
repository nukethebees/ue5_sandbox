#include "Sandbox/combat/pawn_weapon_component/PawnWeaponComponent.h"

#include "Engine/World.h"

#include "Sandbox/combat/weapons/WeaponBase.h"
#include "Sandbox/inventory/InventoryComponent.h"
#include "Sandbox/utilities/enums.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UPawnWeaponComponent::UPawnWeaponComponent() {}

void UPawnWeaponComponent::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(owner, GetOwner());

    inventory = owner->GetComponentByClass<UInventoryComponent>();
    check(inventory);
    inventory->on_weapon_added.BindUObject(this, &UPawnWeaponComponent::on_weapon_added);
    inventory->on_ammo_added.AddRaw(&on_reserve_ammo_changed, &FOnReserveAmmoChanged::Broadcast);
}

bool UPawnWeaponComponent::can_fire() const {
    RETURN_VALUE_IF_NULLPTR(active_weapon, false);
    return active_weapon->can_fire();
}
void UPawnWeaponComponent::start_firing() {
    RETURN_IF_NULLPTR(active_weapon);
    return active_weapon->start_firing();
}
void UPawnWeaponComponent::sustain_firing(float delta_time) {
    if (!active_weapon) {
        return;
    }
    return active_weapon->sustain_firing(delta_time);
}
void UPawnWeaponComponent::stop_firing() {
    if (!active_weapon) {
        return;
    }
    return active_weapon->stop_firing();
}

void UPawnWeaponComponent::reload() {
    RETURN_IF_NULLPTR(active_weapon);
    RETURN_IF_NULLPTR(inventory);

    if (!active_weapon->can_reload()) {
        return;
    }

    auto const ammo_needed{active_weapon->get_ammo_needed_for_reload()};
    auto const ammo_got{inventory->request_ammo(ammo_needed)};
    auto const ammo_left{inventory->count_ammo(ammo_needed.type)};

    active_weapon->reload(ammo_got);

    on_weapon_reloaded.Broadcast(active_weapon->get_current_ammo(), ammo_left);
}

void UPawnWeaponComponent::unequip_weapon() {
    if (!active_weapon) {
        return;
    }

    active_weapon->on_ammo_changed.RemoveAll(this);
    active_weapon->hide_weapon();
    active_weapon = nullptr;

    on_weapon_unequipped.Broadcast();
}
void UPawnWeaponComponent::cycle_next_weapon() {
    constexpr auto logger{NestedLogger<"cycle_next_weapon">()};

    RETURN_IF_NULLPTR(inventory);
    if (!active_weapon) {
        if (auto* weapon{inventory->get_first_weapon()}) {
            equip_weapon(*weapon);
        }
    } else if (auto* weapon{inventory->get_weapon_after(*active_weapon)}) {
        equip_weapon(*weapon);
    }
}
void UPawnWeaponComponent::cycle_prev_weapon() {
    constexpr auto logger{NestedLogger<"cycle_prev_weapon">()};

    RETURN_IF_NULLPTR(inventory);
    if (!active_weapon) {
        if (auto* weapon{inventory->get_last_weapon()}) {
            equip_weapon(*weapon);
        }
    } else if (auto* weapon{inventory->get_weapon_before(*active_weapon)}) {
        equip_weapon(*weapon);
    }
}

void UPawnWeaponComponent::equip_weapon(AWeaponBase& weapon) {
    constexpr auto logger{NestedLogger<"equip_weapon">()};

    unequip_weapon();
    active_weapon = &weapon;
    active_weapon->show_weapon();

    active_weapon->on_ammo_changed.AddRaw(&on_weapon_ammo_changed, &FOnAmmoChanged::Broadcast);

    logger.log_display(TEXT("Equipped weapon: %s"), *active_weapon->GetName());

    auto const reserve_ammo{inventory->count_ammo(active_weapon->get_ammo_type())};

    on_weapon_equipped.Broadcast(
        active_weapon->get_current_ammo(), active_weapon->get_max_ammo(), reserve_ammo);
}
void UPawnWeaponComponent::attach_weapon(AWeaponBase& weapon, USceneComponent& location) {
    constexpr bool weld_to_parent{false};
    weapon.AttachToComponent(
        &location, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, weld_to_parent));
    weapon.SetActorRelativeTransform(FTransform{});
}
void UPawnWeaponComponent::on_weapon_added(AWeaponBase& weapon) {
    constexpr auto logger{NestedLogger<"on_weapon_added">()};

    check(attach_location);
    attach_weapon(weapon, *attach_location);
    weapon.hide_weapon();

    switch (weapon_pickup_action) {
        using enum EWeaponPickupAction;
        case Nothing: {
            break;
        }
        case EquipIfNothingEquipped: {
            if (!active_weapon) {
                equip_weapon(weapon);
            }
            break;
        }
        case Equip: {
            equip_weapon(weapon);
            break;
        }
        default: {
            logger.log_warning(TEXT("%s"),
                               *ml::make_unhandled_enum_case_warning(weapon_pickup_action));
            break;
        }
    }
}
