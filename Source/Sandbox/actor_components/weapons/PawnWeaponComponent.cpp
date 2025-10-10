#include "Sandbox/actor_components/weapons/PawnWeaponComponent.h"

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
    spawn_parameters.Owner = GetOwner();
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
    constexpr auto logger{NestedLogger<"equip_weapon">()};

    RETURN_IF_NULLPTR(weapon);
    RETURN_IF_NULLPTR(attach_location);
    TRY_INIT_PTR(world, GetWorld());

    unequip_weapon();

    logger.log_display(TEXT("Equipping %s"), *weapon->get_name());

    TRY_INIT_PTR(spawned_weapon,
                 world->SpawnActor(weapon->GetClass(), &spawn_transform, spawn_parameters));

    log_display(TEXT("Spawning weapon to %s"), *spawn_transform.ToString());

    active_weapon = Cast<AWeaponBase>(spawned_weapon);
    RETURN_IF_NULLPTR(active_weapon);

    TRY_INIT_PTR(owner, GetOwner());
    active_weapon->SetOwner(owner);

    constexpr bool weld_to_parent{false};
    active_weapon->AttachToComponent(
        attach_location, FAttachmentTransformRules(EAttachmentRule::KeepRelative, weld_to_parent));

    logger.log_display(TEXT("Attached weapon: %s"), *active_weapon->GetName());
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
