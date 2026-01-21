#include "Sandbox/combat/weapons/actors/RocketLauncher.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ARocketLauncher::ARocketLauncher()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))) {
    mesh->SetupAttachment(RootComponent);

    this->size = FDimensions{4, 2};
    this->max_ammo = 1;
    this->ammo_type = EAmmoType::Rockets;
}
bool ARocketLauncher::can_reload() const {
    return ammo < max_ammo;
}
