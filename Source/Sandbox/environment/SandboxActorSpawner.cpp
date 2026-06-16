#include "Sandbox/environment/SandboxActorSpawner.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ASandboxActorSpawner::ASandboxActorSpawner()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))}
    , arrow{CreateDefaultSubobject<UArrowComponent>(TEXT("arrow"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    arrow->SetupAttachment(RootComponent);
    arrow->SetVisibility(true);
    arrow->SetHiddenInGame(false);

    PrimaryActorTick.bCanEverTick = true;
}

void ASandboxActorSpawner::Tick(float dt) {
    Super::Tick(dt);

    auto const n_actors{clear_destroyed_actors()};
    if (n_actors < spawn_limit) { spawn(); }
}
void ASandboxActorSpawner::BeginPlay() {
    Super::BeginPlay();

    if (!actor_class) {
        WARN_IS_FALSE(LogSandboxActor, actor_class);
        SetActorTickEnabled(false);
        return;
    }

    PrimaryActorTick.TickInterval = spawn_period;
    SetActorTickEnabled(true);
}
void ASandboxActorSpawner::EndPlay(EEndPlayReason::Type const reason) {
    Super::EndPlay(reason);

    destroy_all_actors();
}

void ASandboxActorSpawner::stop() {
    SetActorTickEnabled(false);
}

void ASandboxActorSpawner::spawn() {
    RETURN_IF_NULLPTR(arrow);
    TRY_INIT_PTR(world, GetWorld());

    FActorSpawnParameters spawn_params;

    auto const arrow_transform{arrow->GetComponentTransform()};
    auto* new_actor{
        world->SpawnActorDeferred<AActor>(actor_class, arrow_transform, nullptr, nullptr)};
    RETURN_IF_NULLPTR(new_actor);

    FVector origin{};
    FVector box_extent{};
    new_actor->GetActorBounds(false, origin, box_extent);
    auto const arrow_fwd{arrow->GetForwardVector()};
    auto const arrow_pos{arrow->GetComponentLocation()};
    auto const spawn_pos{arrow_pos + arrow_fwd * box_extent};

    FTransform const spawn_transform{GetActorQuat(), spawn_pos, GetActorScale()};

    new_actor->SetLifeSpan(actor_lifespan);
    new_actor->FinishSpawning(spawn_transform);
    configure_instance(*new_actor);

    spawned_actors.Add(new_actor);
}
void ASandboxActorSpawner::configure_instance(AActor& instance) {}

void ASandboxActorSpawner::destroy_all_actors() {
    for (auto* actor : spawned_actors) {
        if (IsValid(actor)) { actor->Destroy(); }
    }

    spawned_actors.Empty();
}
auto ASandboxActorSpawner::clear_destroyed_actors() -> int32 {
    spawned_actors.RemoveAll([](AActor* actor) { return !IsValid(actor); });
    return spawned_actors.Num();
}
