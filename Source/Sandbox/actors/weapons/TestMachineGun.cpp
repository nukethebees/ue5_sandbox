#include "Sandbox/actors/weapons/TestMachineGun.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/subsystems/world/MassBulletSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

ATestMachineGun::ATestMachineGun()
    : ammo{max_ammo} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    gun_mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMeshComponent"));
    gun_mesh_component->SetupAttachment(RootComponent);

    fire_point_arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FirePoint"));
    fire_point_arrow->SetupAttachment(RootComponent);
}

bool ATestMachineGun::can_fire() const {
    return ammo > 0;
}

bool ATestMachineGun::can_reload() const {
    return ammo < max_ammo;
}

int32 ATestMachineGun::get_ammo_needed() const {
    return max_ammo - ammo;
}

FAmmoData ATestMachineGun::get_current_ammo() const {
    return FAmmoData::make_discrete(EAmmoType::Bullets, ammo);
}

FAmmoData ATestMachineGun::get_max_ammo() const {
    return FAmmoData::make_discrete(EAmmoType::Bullets, max_ammo);
}

FAmmoReloadResult ATestMachineGun::reload(FAmmoData const& offered) {
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

void ATestMachineGun::start_firing() {
    is_firing = true;
    time_since_last_shot = 0.0f;
}

void ATestMachineGun::sustain_firing(float delta_time) {
    if (!is_firing) {
        return;
    }

    if (!can_fire()) {
        return;
    }

    time_since_last_shot += delta_time;
    auto const seconds_per_bullet{1.0f / fire_rate};

    while (time_since_last_shot >= seconds_per_bullet && can_fire()) {
        time_since_last_shot -= seconds_per_bullet;
        fire_single_bullet(time_since_last_shot);
    }
}

void ATestMachineGun::stop_firing() {
    is_firing = false;
    time_since_last_shot = 0.0f;
}

void ATestMachineGun::fire_single_bullet(float travel_time) {
    RETURN_IF_NULLPTR(fire_point_arrow);
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_bullet_subsystem, world->GetSubsystem<UMassBulletSubsystem>());

    FTransform const spawn_transform{get_spawn_transform(travel_time)};
    mass_bullet_subsystem->add_bullet(spawn_transform, bullet_speed);

    ammo -= 1;
}

FTransform ATestMachineGun::get_spawn_transform(float travel_time) const {
    auto const spawn_rotation{fire_point_arrow->GetComponentRotation()};
    auto const travel_offset{spawn_rotation.Vector() * bullet_speed * travel_time};
    auto const spawn_location{fire_point_arrow->GetComponentLocation() + travel_offset};
    auto const spawn_scale{FVector::OneVector};

    FTransform const spawn_transform{spawn_rotation, spawn_location, spawn_scale};

    return spawn_transform;
}

UStaticMesh* ATestMachineGun::get_display_mesh() const {
    RETURN_VALUE_IF_NULLPTR(gun_mesh_component, nullptr);
    return gun_mesh_component->GetStaticMesh();
}
