#include "Sandbox/combat/weapons/actors/TestPistol.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/combat/projectiles/data_assets/BulletDataAsset.h"
#include "Sandbox/combat/projectiles/subsystems/MassBulletSubsystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ATestPistol::ATestPistol()
    : gun_mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMeshComponent"))}
    , fire_point_arrow{CreateDefaultSubobject<UArrowComponent>(TEXT("FirePoint"))} {
    ammo_type = EAmmoType::Bullets;
    max_ammo = 10;
    ammo = max_ammo;

    size = FDimensions{4, 1};

    gun_mesh_component->SetupAttachment(RootComponent);
    fire_point_arrow->SetupAttachment(RootComponent);
}

// IWeaponInterface
bool ATestPistol::can_fire() const {
    return ammo > 0;
}
void ATestPistol::start_firing() {
    if (!can_fire()) {
        return;
    }

    RETURN_IF_NULLPTR(fire_point_arrow);
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_bullet_subsystem, world->GetSubsystem<UMassBulletSubsystem>());

    auto const spawn_rotation{fire_point_arrow->GetComponentRotation()};
    auto const spawn_location{fire_point_arrow->GetComponentLocation()};
    auto const spawn_scale{FVector::OneVector};

    FTransform const spawn_transform{spawn_rotation, spawn_location, spawn_scale};

    if (cached_bullet_type_index.IsSet()) {
        mass_bullet_subsystem->add_bullet(
            spawn_transform, bullet_speed, cached_bullet_type_index.GetValue());
    } else {
        RETURN_IF_NULLPTR(bullet_data);
        mass_bullet_subsystem->add_bullet(
            spawn_transform, bullet_speed, bullet_data->GetPrimaryAssetId());
    }

    ammo -= 1;
    on_ammo_changed.Broadcast(get_current_ammo());
}

FAmmoReloadResult ATestPistol::reload(FAmmoData const& offered) {
    FAmmoReloadResult result;
    result.AmmoOfferedRemaining = offered;

    if (offered.type != get_ammo_type()) {
        return result;
    }

    if (!can_reload()) {
        return result;
    }

    auto const needed{get_ammo_needed()};
    auto const taken{FMath::Min(needed, offered.amount)};

    ammo += taken;

    result.AmmoTaken = FAmmoData(EAmmoType::Bullets, taken);
    result.AmmoOfferedRemaining.amount -= taken;

    on_ammo_changed.Broadcast(get_current_ammo());

    return result;
}
bool ATestPistol::can_reload() const {
    return ammo < max_ammo;
}
FAmmoData ATestPistol::get_ammo_needed_for_reload() const {
    return FAmmoData{ammo_type, max_ammo - ammo};
}

UStaticMesh* ATestPistol::get_display_mesh() const {
    RETURN_VALUE_IF_NULLPTR(gun_mesh_component, nullptr);
    return gun_mesh_component->GetStaticMesh();
}

// IInventoryItem
int32 ATestPistol::get_ammo_needed() const {
    return max_ammo - ammo;
}

void ATestPistol::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_bullet_subsystem, world->GetSubsystem<UMassBulletSubsystem>());
    RETURN_IF_NULLPTR(bullet_data);

    cached_bullet_type_index =
        mass_bullet_subsystem->get_bullet_type_index(bullet_data->GetPrimaryAssetId());

    on_ammo_changed.Broadcast(get_current_ammo());
}
