#include "TestCapitalShipFighters.h"

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
}
void ATestCapitalShipFighters::PostInitializeComponents() {
    Super::PostInitializeComponents();

    clear_runtime_state();
}
void ATestCapitalShipFighters::BeginPlay() {
    Super::BeginPlay();
}
void ATestCapitalShipFighters::Tick(float dt) {
    Super::Tick(dt);
}

// Spawning
void ATestCapitalShipFighters::spawn_instance(FTransform const& transform) {}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    instances->ClearInstances();
}
