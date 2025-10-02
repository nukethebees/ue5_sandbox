#include "Sandbox/actors/MassBulletSpawner.h"

#include "Components/ArrowComponent.h"
#include "MassEntitySubsystem.h"

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

    if (!bullet_class) {
        log_warning(TEXT("bullet_class is nullptr."));
        return;
    }
    if (!fire_point) {
        log_warning(TEXT("fire_point is nullptr."));
        return;
    }

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(game_instance, GetGameInstance());
    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    TRY_INIT_PTR(archetype_subsystem, game_instance->GetSubsystem<UMassArchetypeSubsystem>());

    auto bullet_archetype{archetype_subsystem->get_bullet_archetype()};
    if (!bullet_archetype.IsValid()) {
        logger.log_verbose(TEXT("bullet_archetype is invalid."));
        return;
    }

    logger.log_verbose(TEXT("Creating entity"));
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto entity = entity_manager.CreateEntity(bullet_archetype);

    auto const spawn_location{fire_point->GetComponentLocation()};
    auto const spawn_rotation{fire_point->GetComponentRotation()};

    auto& transform_frag{
        entity_manager.GetFragmentDataChecked<FMassBulletTransformFragment>(entity)};
    transform_frag.transform = FTransform(spawn_location);

    auto& velocity_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletVelocityFragment>(entity);
    auto const velocity{spawn_rotation.Vector() * bullet_speed};
    velocity_frag.velocity = velocity;

    //  auto & index_frag =
    //  entity_manager.GetFragmentDataChecked<FMassBulletInstanceIndexFragment>(entity); if (auto
    //  idx = visualization_component->add_instance(FTransform(position))) {
    //      index_frag.instance_index = *idx;
    //  }
}
