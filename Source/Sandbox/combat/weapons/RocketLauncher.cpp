#include "Sandbox/combat/weapons/RocketLauncher.h"

#include "Sandbox/combat/weapons/Rocket.h"
#include "Sandbox/combat/weapons/RocketConfig.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ARocketLauncher::ARocketLauncher()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))) {
    mesh->SetupAttachment(RootComponent);

    this->size = FDimensions{4, 2};
    this->max_ammo = 1;
    this->ammo_type = EAmmoType::Rockets;
}

bool ARocketLauncher::can_fire() const {
    return ammo > 0;
}
void ARocketLauncher::start_firing() {
    if (!can_fire()) {
        return;
    }

    FName const socket_name{TEXT("Fire")};

    RETURN_IF_NULLPTR(rocket_class);
    RETURN_IF_NULLPTR(mesh);
    RETURN_IF_FALSE(mesh->DoesSocketExist(socket_name));
    TRY_INIT_PTR(world, GetWorld());

    auto const socket_transform{mesh->GetSocketTransform(socket_name, RTS_World)};

    UE_LOG(LogSandboxActor,
           Log,
           TEXT("Socket transform: %s"),
           *socket_transform.ToHumanReadableString());

    FActorSpawnParameters spawn_params;
    spawn_params.Owner = this;
    spawn_params.Instigator = GetInstigator();

    TRY_INIT_PTR(rocket, world->SpawnActor<ARocket>(rocket_class, socket_transform, spawn_params));
    rocket->fire({rocket_speed, explosion_radius, projectile_damage});

    ammo -= 1;
    on_ammo_changed.Broadcast(get_current_ammo());
}
bool ARocketLauncher::can_reload() const {
    return ammo < max_ammo;
}
