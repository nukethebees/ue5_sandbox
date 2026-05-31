#include "TestCapitalShipFighters.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestCapitalShipFightersConfig.h"
#include "TestLasers.h"

#include <SandboxCore/Public/array_math.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>

ATestCapitalShipFighters::ATestCapitalShipFighters()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Actor life cycle
void ATestCapitalShipFighters::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!actor_config) {
        return;
    }

    instances->SetStaticMesh(actor_config->mesh);
}
void ATestCapitalShipFighters::PostInitializeComponents() {
    Super::PostInitializeComponents();

    clear_runtime_state();
}
void ATestCapitalShipFighters::BeginPlay() {
    Super::BeginPlay();

    if (!instances->GetStaticMesh()) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: mesh is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    if (!actor_config) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: actor_config is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    if (!laser_actor) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: laser_actor is nullptr"));
        SetActorTickEnabled(false);
        return;
    }
}
void ATestCapitalShipFighters::Tick(float dt) {
    Super::Tick(dt);

    laser_cooldowns.tick(dt);

    move_ships(dt);
    handle_firing();
}

// Getters
auto ATestCapitalShipFighters::get_team() const noexcept -> ETestTeam {
    return team;
}
auto ATestCapitalShipFighters::get_num_instances() const noexcept -> int32 {
    return world_transforms.Num();
}

// Movement
void ATestCapitalShipFighters::move_ships(float const dt) {
    auto const n{get_num_instances()};
    auto const speed{actor_config->speed};

    for (int32 i{0}; i < n; ++i) {
        auto const fwd{world_transforms[i].GetRotation().Vector()};
        auto const velocity{fwd * speed};
        auto const delta_move{velocity * dt};
        world_transforms[i].AddToTranslation(delta_move);
    }

    instances->BatchUpdateInstancesTransforms(0, world_transforms, true, true);
}

// Spawning
void ATestCapitalShipFighters::spawn_instance(FTransform const& transform) {
    if (!actor_config) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestCapitalShipFighters: actor_config is nullptr"));
        return;
    }

    instances->AddInstance(transform, true);

    world_transforms.Add(transform);
    healths.Add(actor_config->health);
    laser_cooldowns.Add(0.f);
}

// Combat
void ATestCapitalShipFighters::handle_firing() {
    auto const n_ships{get_num_instances()};
    indices_ready_to_fire.SetNumUninitialized(n_ships, EAllowShrinking::No);
    auto const cooldown{actor_config->fire_cooldown};

    auto const n_to_fire{ml::collect_indices_less_equal(
        TConstArrayView<float>{laser_cooldowns.remaining_times}, 0.f, indices_ready_to_fire)};

    for (int32 i{0}; i < n_to_fire; ++i) {
        auto const ship_index{indices_ready_to_fire[i]};

        laser_actor->spawn_laser(world_transforms[i], *this);
        laser_cooldowns.remaining_times[ship_index] = cooldown;
    }
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    instances->ClearInstances();
}
