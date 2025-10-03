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

    constexpr auto logger{NestedLogger<"spawn_bullet">()};

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(archetype_subsystem, world->GetSubsystem<UMassArchetypeSubsystem>());
    bullet_archetype = archetype_subsystem->get_bullet_archetype();

    for (auto it{TActorIterator<AMassBulletVisualizationActor>(world)}; it; ++it) {
        visualization_actor = *it;
        break;
    }
    if (!visualization_actor) {
        logger.log_warning(TEXT("visualization_actor is nullptr."));
    }

    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};

    auto impact_effect_handle{
        entity_manager.GetOrCreateConstSharedFragment<FMassBulletImpactEffectFragment>(
            bullet_data->impact_effect)};

    auto viz_actor_handle{
        entity_manager.GetOrCreateConstSharedFragment<FMassBulletVisualizationActorFragment>(
            visualization_actor)};

    shared_values.Add(impact_effect_handle);
    shared_values.Add(viz_actor_handle);
    shared_values.Sort();
}
void AMassBulletSpawner::Tick(float DeltaTime) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletSpawner::Tick"))

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
    RETURN_IF_NULLPTR(visualization_actor);

    TRY_INIT_PTR(world, GetWorld());

    FTransform const spawn_transform{get_spawn_transform(*fire_point)};

    TRY_INIT_OPTIONAL(idx, visualization_actor->add_instance(spawn_transform));

    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto& cmd_buffer{entity_manager.Defer()};

    cmd_buffer.PushCommand<FMassDeferredCreateCommand>(
        [idx, spawn_transform, this](FMassEntityManager& entity_manager) {
            logger.log_verbose(TEXT("Async creation"));

            auto entity = entity_manager.CreateEntity(bullet_archetype, shared_values);
            entity_manager.AddTagToEntity(entity, FMassBulletActiveTag::StaticStruct());

            auto& transform_frag{
                entity_manager.GetFragmentDataChecked<FMassBulletTransformFragment>(entity)};
            transform_frag.transform = spawn_transform;

            auto& velocity_frag =
                entity_manager.GetFragmentDataChecked<FMassBulletVelocityFragment>(entity);
            auto const velocity{spawn_transform.Rotator().Vector().GetSafeNormal() * bullet_speed};
            velocity_frag.velocity = velocity;

            auto& index_frag =
                entity_manager.GetFragmentDataChecked<FMassBulletInstanceIndexFragment>(entity);
            index_frag.instance_index = *idx;

            auto& last_pos_frag =
                entity_manager.GetFragmentDataChecked<FMassBulletLastPositionFragment>(entity);
            last_pos_frag.last_position = spawn_transform.GetLocation();

            auto& hit_info_frag =
                entity_manager.GetFragmentDataChecked<FMassBulletHitInfoFragment>(entity);
            hit_info_frag.hit_location = FVector::ZeroVector;
            hit_info_frag.hit_normal = FVector::ZeroVector;
        });
}
FTransform AMassBulletSpawner::get_spawn_transform(UArrowComponent const& bullet_start) const {
    auto const spawn_location{bullet_start.GetComponentLocation()};
    auto const spawn_rotation{bullet_start.GetComponentRotation()};
    FVector const spawn_scale{FVector::OneVector};

    FTransform const spawn_transform{spawn_rotation, spawn_location, spawn_scale};

    return spawn_transform;
}
