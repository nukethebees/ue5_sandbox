#include "Sandbox/actors/weapons/TestPistol.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/subsystems/world/MassBulletSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

ATestPistol::ATestPistol() {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    gun_mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMeshComponent"));
    gun_mesh_component->SetupAttachment(RootComponent);

    fire_point_arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FirePoint"));
    fire_point_arrow->SetupAttachment(RootComponent);
}

void ATestPistol::start_firing() {
    RETURN_IF_NULLPTR(fire_point_arrow);
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_bullet_subsystem, world->GetSubsystem<UMassBulletSubsystem>());

    constexpr float bullet_speed{5000.0f};

    auto const spawn_rotation{fire_point_arrow->GetComponentRotation()};
    auto const spawn_location{fire_point_arrow->GetComponentLocation()};
    auto const spawn_scale{FVector::OneVector};

    FTransform const spawn_transform{spawn_rotation, spawn_location, spawn_scale};
    mass_bullet_subsystem->add_bullet(spawn_transform, bullet_speed);
}

UStaticMesh* ATestPistol::get_display_mesh() const {
    RETURN_VALUE_IF_NULLPTR(gun_mesh_component, nullptr);
    return gun_mesh_component->GetStaticMesh();
}
