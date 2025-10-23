#include "Sandbox/combat/weapons/actor_components/PawnWeaponComponent.h"

#include "Sandbox/combat/weapons/actor_components/WeaponComponent.h"
#include "Sandbox/combat/weapons/actors/WeaponBase.h"
#include "Sandbox/inventory/actor_components/InventoryComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UPawnWeaponComponent::UPawnWeaponComponent()
    : spawn_parameters() {
    FVector const translation{0.0f, 0.0f, 200.0f};
    auto const rotation{FRotator{}};
    auto const scale{FVector::OneVector};
    spawn_transform = FTransform{rotation, translation, scale};

    spawn_parameters.Name = TEXT("SpawnedGun");
    spawn_parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
}

void UPawnWeaponComponent::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(owner, GetOwner());

    spawn_parameters.Name = TEXT("SpawnedGun");
    spawn_parameters.Owner = owner;

    inventory = owner->GetComponentByClass<UInventoryComponent>();
    check(inventory);
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
    active_weapon->reload(FAmmoData::make_discrete(EAmmoType::Bullets, 100));
}
bool UPawnWeaponComponent::can_reload() const {
    RETURN_VALUE_IF_NULLPTR(active_weapon, false);
    return active_weapon->can_reload();
}

void UPawnWeaponComponent::equip_weapon(AWeaponBase* weapon) {
    constexpr auto logger{NestedLogger<"equip_weapon">()};

    RETURN_IF_NULLPTR(weapon);

    unequip_weapon();
    active_weapon = weapon;
    active_weapon->show_weapon();

    active_weapon->on_ammo_changed.AddDynamic(this,
                                              &UPawnWeaponComponent::on_active_weapon_ammo_changed);
    on_weapon_ammo_changed.Broadcast(active_weapon->get_current_ammo());

    logger.log_display(TEXT("Equipped weapon: %s"), *active_weapon->GetName());
}
void UPawnWeaponComponent::unequip_weapon() {
    if (!active_weapon) {
        return;
    }

    active_weapon->on_ammo_changed.RemoveDynamic(
        this, &UPawnWeaponComponent::on_active_weapon_ammo_changed);
    active_weapon->hide_weapon();
    active_weapon = nullptr;
}
void UPawnWeaponComponent::cycle_next_weapon() {
    RETURN_IF_NULLPTR(inventory);
    TRY_INIT_PTR(weapon, inventory->get_random_weapon());
    equip_weapon(weapon);
}
void UPawnWeaponComponent::cycle_prev_weapon() {
    RETURN_IF_NULLPTR(inventory);
    TRY_INIT_PTR(weapon, inventory->get_random_weapon());
    equip_weapon(weapon);
}

bool UPawnWeaponComponent::pickup_new_weapon(TSubclassOf<AWeaponBase> weapon_class) {
    constexpr auto logger{NestedLogger<"pickup_new_weapon">()};

    logger.log_display(TEXT("Picking up new weapon"));

    INIT_PTR_OR_RETURN_VALUE(owner, GetOwner(), false);
    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), false);
    INIT_PTR_OR_RETURN_VALUE(weapon, spawn_weapon(weapon_class, *owner, *world), false);

    RETURN_VALUE_IF_NULLPTR(attach_location, false);
    INIT_PTR_OR_RETURN_VALUE(
        inventory_component, owner->GetComponentByClass<UInventoryComponent>(), false);

    if (!pickup_new_weapon(*weapon, *inventory_component, *attach_location)) {
        weapon->Destroy();
        return false;
    }

    return true;
}
bool UPawnWeaponComponent::pickup_new_weapon(AWeaponBase& weapon) {
    INIT_PTR_OR_RETURN_VALUE(owner, GetOwner(), false);
    RETURN_VALUE_IF_NULLPTR(attach_location, false);
    RETURN_VALUE_IF_NULLPTR(inventory, false);

    return pickup_new_weapon(weapon, *inventory, *attach_location);
}
bool UPawnWeaponComponent::pickup_new_weapon(AWeaponBase& weapon,
                                             UInventoryComponent& inventory_component,
                                             USceneComponent& location) {
    constexpr auto logger{NestedLogger<"pickup_new_weapon">()};

    if (!inventory_component.add_item(&weapon)) {
        return false;
    }

    attach_weapon(weapon, location);
    weapon.hide_weapon();

    log_display(TEXT("Picked up weapon: %s"), *weapon.get_name());

    switch (weapon_pickup_action) {
        using enum EWeaponPickupAction;
        case Nothing: {
            break;
        }
        case EquipIfNothingEquipped: {
            if (!active_weapon) {
                equip_weapon(&weapon);
            }
            break;
        }
        case Equip: {
            equip_weapon(&weapon);
            break;
        }
        default: {
            logger.log_warning(TEXT("Unhandled enum value"));
            break;
        }
    }

    return true;
}
void UPawnWeaponComponent::attach_weapon(AWeaponBase& weapon, USceneComponent& location) {
    constexpr bool weld_to_parent{false};
    weapon.AttachToComponent(
        &location, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, weld_to_parent));
    weapon.SetActorRelativeTransform(FTransform{});
}
AWeaponBase* UPawnWeaponComponent::spawn_weapon(TSubclassOf<AWeaponBase> weapon_class,
                                                AActor& owner,
                                                UWorld& world) {
    auto* spawned_weapon{
        world.SpawnActor<AWeaponBase>(weapon_class, spawn_transform, spawn_parameters)};
    RETURN_VALUE_IF_NULLPTR(spawned_weapon, nullptr);
    spawned_weapon->SetOwner(&owner);

    return spawned_weapon;
}

void UPawnWeaponComponent::set_attach_location(USceneComponent* new_value) {
    constexpr auto logger{NestedLogger<"set_attach_location">()};

    RETURN_IF_NULLPTR(new_value);
    attach_location = new_value;
    spawn_transform.SetLocation(attach_location->GetRelativeLocation());
}

void UPawnWeaponComponent::on_active_weapon_ammo_changed(FAmmoData current_ammo) {
    on_weapon_ammo_changed.Broadcast(current_ammo);
}
