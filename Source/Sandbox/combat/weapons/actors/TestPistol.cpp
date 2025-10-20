#include "Sandbox/actors/weapons/TestPistol.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/subsystems/world/MassBulletSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

ATestPistol::ATestPistol()
    : ammo{max_ammo} {
    gun_mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMeshComponent"));
    gun_mesh_component->SetupAttachment(RootComponent);

    fire_point_arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FirePoint"));
    fire_point_arrow->SetupAttachment(RootComponent);
}

FDimensions ATestPistol::get_size() const {
    return FDimensions{4, 1};
};

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
    auto const taken{FMath::Min(needed, offered.discrete_amount)};

    ammo += taken;

    result.AmmoTaken = FAmmoData::make_discrete(EAmmoType::Bullets, taken);
    result.AmmoOfferedRemaining.discrete_amount -= taken;

    return result;
}
bool ATestPistol::can_reload() const {
    return ammo < max_ammo;
}

FAmmoData ATestPistol::get_current_ammo() const {
    return FAmmoData::make_discrete(EAmmoType::Bullets, ammo);
}
FAmmoData ATestPistol::get_max_ammo() const {
    return FAmmoData::make_discrete(EAmmoType::Bullets, max_ammo);
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
}
