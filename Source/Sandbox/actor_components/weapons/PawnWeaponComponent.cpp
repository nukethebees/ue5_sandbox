#include "Sandbox/actor_components/weapons/PawnWeaponComponent.h"

#include "Sandbox/actor_components/inventory/InventoryComponent.h"
#include "Sandbox/actor_components/weapons/WeaponComponent.h"
#include "Sandbox/actors/weapons/WeaponBase.h"

#include "Sandbox/macros/null_checks.hpp"

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

    spawn_parameters.Name = TEXT("SpawnedGun");
    spawn_parameters.Owner = GetOwner();
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
    return active_weapon->reload();
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

    logger.log_display(TEXT("Equipped weapon: %s"), *active_weapon->GetName());
}
void UPawnWeaponComponent::unequip_weapon() {
    if (!active_weapon) {
        return;
    }

    active_weapon->hide_weapon();
    active_weapon = nullptr;
}
bool UPawnWeaponComponent::pickup_new_weapon(TSubclassOf<AWeaponBase> weapon_class) {
    constexpr auto logger{NestedLogger<"pickup_new_weapon">()};

    logger.log_display(TEXT("Picking up new weapon"));

    RETURN_VALUE_IF_NULLPTR(attach_location, false);
    INIT_PTR_OR_RETURN_VALUE(owner, GetOwner(), false);
    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), false);
    INIT_PTR_OR_RETURN_VALUE(
        inventory_component, owner->GetComponentByClass<UInventoryComponent>(), false);
    INIT_PTR_OR_RETURN_VALUE(weapon, spawn_weapon(weapon_class, *owner, *world), false);

    attach_weapon(*weapon, *attach_location);
    weapon->hide_weapon();

    if (!inventory_component->add_item(weapon)) {
        weapon->Destroy();
        return false;
    }

    logger.log_display(TEXT("Picked up weapon: %s"), *weapon->get_name());
    return true;
}

void UPawnWeaponComponent::attach_weapon(AWeaponBase& weapon, USceneComponent& location) {
    constexpr bool weld_to_parent{false};
    weapon.AttachToComponent(
        &location, FAttachmentTransformRules(EAttachmentRule::KeepRelative, weld_to_parent));
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

    logger.log_display(TEXT("Setting new attach location."));

    RETURN_IF_NULLPTR(new_value);
    TRY_INIT_PTR(scene_owner, new_value->GetOwner());

    if (scene_owner != GetOwner()) {
        logger.log_error(TEXT("Scene component's owner is different."));
        return;
    }

    attach_location = new_value;
    spawn_transform.SetLocation(attach_location->GetRelativeLocation());
}
