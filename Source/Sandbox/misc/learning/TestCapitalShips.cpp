#include "TestCapitalShips.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include <SandboxCore/Public/array_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>

ATestCapitalShips::ATestCapitalShips()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Actor Lifecycle
void ATestCapitalShips::Tick(float dt) {
    Super::Tick(dt);
}
void ATestCapitalShips::BeginPlay() {
    Super::BeginPlay();
}

// Spawning
void ATestCapitalShips::spawn_ship(FTransform const& proxy) {}
