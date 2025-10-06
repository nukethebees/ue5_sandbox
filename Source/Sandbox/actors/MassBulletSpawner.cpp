#include "Sandbox/actors/MassBulletSpawner.h"

#include "Components/ArrowComponent.h"
#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassEntitySubsystem.h"

#include "Sandbox/actors/BulletActor.h"
#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/subsystems/world/MassArchetypeSubsystem.h"
#include "Sandbox/subsystems/world/MassBulletSubsystem.h"
#include "Sandbox/subsystems/world/ObjectPoolSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

AMassBulletSpawner::AMassBulletSpawner() {
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    fire_point_component_ = CreateDefaultSubobject<UArrowComponent>(TEXT("fire_point_component_"));
    fire_point_component_->SetupAttachment(RootComponent);
}
void AMassBulletSpawner::BeginPlay() {
    Super::BeginPlay();
}
void AMassBulletSpawner::Tick(float DeltaTime) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletSpawner::Tick"))

    Super::Tick(DeltaTime);

    RETURN_IF_NULLPTR(fire_point_component_);
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_bullet_subsystem, world->GetSubsystem<UMassBulletSubsystem>());

    time_since_last_shot += DeltaTime;
    auto const seconds_per_bullet{1.0f / bullets_per_second};

    while (time_since_last_shot >= seconds_per_bullet) {
        time_since_last_shot -= seconds_per_bullet;
        spawn_bullet(*mass_bullet_subsystem, *fire_point_component_, time_since_last_shot);
    }
}

void AMassBulletSpawner::spawn_bullet(UMassBulletSubsystem& mass_bullet_subsystem,
                                      UArrowComponent const& fire_point,
                                      float travel_time) {
    FTransform const spawn_transform{get_spawn_transform(fire_point, travel_time)};
    mass_bullet_subsystem.add_bullet(spawn_transform, bullet_speed);
}
FTransform AMassBulletSpawner::get_spawn_transform(UArrowComponent const& fire_point,
                                                   float travel_time) const {
    auto const spawn_rotation{fire_point.GetComponentRotation()};
    auto const travel_offset{spawn_rotation.Vector() * bullet_speed * travel_time};
    auto const spawn_location{fire_point.GetComponentLocation() + travel_offset};
    auto const spawn_scale{FVector::OneVector};

    FTransform const spawn_transform{spawn_rotation, spawn_location, spawn_scale};

    return spawn_transform;
}
