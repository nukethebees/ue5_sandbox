#include "Sandbox/actors/weapons/TestPistol.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/macros/null_checks.hpp"

ATestPistol::ATestPistol() {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    gun_mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMeshComponent"));
    gun_mesh_component->SetupAttachment(RootComponent);

    fire_point_arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FirePoint"));
    fire_point_arrow->SetupAttachment(RootComponent);
}

void ATestPistol::start_firing() {}
void ATestPistol::sustain_firing(float DeltaTime) {}
void ATestPistol::stop_firing() {}

void ATestPistol::reload() {}
bool ATestPistol::can_reload() const {
    return false;
}

bool ATestPistol::can_fire() const {
    return false;
}
EAmmoType ATestPistol::get_ammo_type() const {
    return EAmmoType::Bullets;
}

FAmmo ATestPistol::get_current_ammo() const {
    return FAmmo{};
}
FAmmo ATestPistol::get_max_ammo() const {
    return FAmmo{};
}

UStaticMesh* ATestPistol::get_display_mesh() const {
    RETURN_VALUE_IF_NULLPTR(gun_mesh_component, nullptr);
    return gun_mesh_component->GetStaticMesh();
}
