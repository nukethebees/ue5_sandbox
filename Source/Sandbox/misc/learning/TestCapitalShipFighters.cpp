#include "TestCapitalShipFighters.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestCapitalShipFightersConfig.h"

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
}
void ATestCapitalShipFighters::Tick(float dt) {
    Super::Tick(dt);

    move_ships(dt);
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
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    instances->ClearInstances();
}
