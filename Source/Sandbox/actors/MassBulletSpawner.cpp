#include "Sandbox/actors/MassBulletSpawner.h"

#include "Components/ArrowComponent.h"
#include "MassEntitySubsystem.h"

#include "Sandbox/actor_components/MassBulletVisualizationComponent.h"
#include "Sandbox/actors/BulletActor.h"
#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/subsystems/game_instance/MassArchetypeSubsystem.h"
#include "Sandbox/subsystems/world/ObjectPoolSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

AMassBulletSpawner::AMassBulletSpawner() {
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    fire_point = CreateDefaultSubobject<UArrowComponent>(TEXT("fire_point"));
    fire_point->SetupAttachment(RootComponent);

    visualisation_component = CreateDefaultSubobject<UMassBulletVisualizationComponent>(
        TEXT("mass_visualisation_component"));
}
void AMassBulletSpawner::BeginPlay() {
    Super::BeginPlay();
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

    RETURN_IF_NULLPTR(bullet_class);
    RETURN_IF_NULLPTR(fire_point);
    RETURN_IF_NULLPTR(visualisation_component);

    auto const spawn_location{fire_point->GetComponentLocation()};
    auto const spawn_rotation{fire_point->GetComponentRotation()};
    FVector const spawn_scale{0.01f, 0.01f, 0.01f};

    FTransform const spawn_transform{spawn_rotation, spawn_location, spawn_scale};

    logger.log_verbose(TEXT("Creating entity"));
    TRY_INIT_OPTIONAL(idx, visualisation_component->add_instance(spawn_transform));

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(game_instance, GetGameInstance());
    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    TRY_INIT_PTR(archetype_subsystem, game_instance->GetSubsystem<UMassArchetypeSubsystem>());

    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    TRY_INIT_VALID(bullet_archetype, archetype_subsystem->get_bullet_archetype());
    auto entity = entity_manager.CreateEntity(bullet_archetype);

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

    auto& viz_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletVisualizationComponentFragment>(entity);
    viz_frag.component = visualisation_component;
}
