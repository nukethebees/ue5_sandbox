#include "Sandbox/combat/bullets/MassBulletSpawner.h"

#include "Components/ArrowComponent.h"
#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassEntitySubsystem.h"

#include "Sandbox/combat/bullets/BulletActor.h"
#include "Sandbox/combat/bullets/BulletDataAsset.h"
#include "Sandbox/combat/bullets/MassBulletFragments.h"
#include "Sandbox/combat/bullets/MassBulletSubsystem.h"
#include "Sandbox/combat/bullets/MassBulletVisualizationActor.h"
#include "Sandbox/core/object_pooling/data/PoolConfig.h"
#include "Sandbox/core/object_pooling/subsystems/ObjectPoolSubsystem.h"
#include "Sandbox/mass_entity/MassArchetypeSubsystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

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
    RETURN_IF_NULLPTR(bullet_data);
    FTransform const spawn_transform{get_spawn_transform(fire_point, travel_time)};
    mass_bullet_subsystem.add_bullet(
        {{spawn_transform, bullet_speed, FHealthChange{10.0f, EHealthChangeType::Damage}, this},
         bullet_data->GetPrimaryAssetId()});
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
