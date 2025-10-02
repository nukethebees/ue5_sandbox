#include "Sandbox/actors/MassBulletSpawner.h"

#include "Components/ArrowComponent.h"
#include "EngineUtils.h"
#include "MassEntitySubsystem.h"

#include "Engine/AssetManager.h"
#include "Sandbox/actors/BulletActor.h"
#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/subsystems/world/MassArchetypeSubsystem.h"
#include "Sandbox/subsystems/world/ObjectPoolSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

AMassBulletSpawner::AMassBulletSpawner() {
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    fire_point = CreateDefaultSubobject<UArrowComponent>(TEXT("fire_point"));
    fire_point->SetupAttachment(RootComponent);
}
void AMassBulletSpawner::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(archetype_subsystem, world->GetSubsystem<UMassArchetypeSubsystem>());
    bullet_archetype = archetype_subsystem->get_bullet_archetype();
}
void AMassBulletSpawner::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    time_since_last_shot += DeltaTime;
    auto const seconds_per_bullet{1.0f / bullets_per_second};

    if (time_since_last_shot >= seconds_per_bullet) {
        spawn_bullet();
        time_since_last_shot = 0.0f;
    }
}

void AMassBulletSpawner::spawn_bullet() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletSpawner::spawn_bullet"))
    constexpr auto logger{NestedLogger<"spawn_bullet">()};

    RETURN_IF_NULLPTR(fire_point);
    RETURN_IF_NULLPTR(bullet_data);
    RETURN_IF_INVALID(bullet_archetype);

    TRY_INIT_PTR(world, GetWorld());
    AMassBulletVisualizationActor * visualization_actor{nullptr};
    for (auto it{TActorIterator<AMassBulletVisualizationActor>(world)}; it; ++it) {
        visualization_actor = *it;
        break;
    }
    RETURN_IF_NULLPTR(visualization_actor);

    auto const spawn_location{fire_point->GetComponentLocation()};
    auto const spawn_rotation{fire_point->GetComponentRotation()};
    FVector const spawn_scale{FVector::OneVector};

    FTransform const spawn_transform{spawn_rotation, spawn_location, spawn_scale};

    TRY_INIT_OPTIONAL(idx, visualization_actor->add_instance(spawn_transform));

    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto entity = entity_manager.CreateEntity(bullet_archetype);

    auto shared_handle{
        entity_manager.GetOrCreateConstSharedFragment<FMassBulletImpactEffectFragment>(
            bullet_data->impact_effect)};
    entity_manager.AddConstSharedFragmentToEntity(entity, shared_handle);

    auto& transform_frag{
        entity_manager.GetFragmentDataChecked<FMassBulletTransformFragment>(entity)};
    transform_frag.transform = spawn_transform;

    auto& velocity_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletVelocityFragment>(entity);
    auto const velocity{spawn_rotation.Vector() * bullet_speed};
    velocity_frag.velocity = velocity;

    auto& index_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletInstanceIndexFragment>(entity);
    index_frag.instance_index = *idx;

    auto& last_pos_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletLastPositionFragment>(entity);
    last_pos_frag.last_position = spawn_location;

    auto& hit_info_frag = entity_manager.GetFragmentDataChecked<FMassBulletHitInfoFragment>(entity);
    hit_info_frag.hit_location = FVector::ZeroVector;
    hit_info_frag.hit_normal = FVector::ZeroVector;
}
