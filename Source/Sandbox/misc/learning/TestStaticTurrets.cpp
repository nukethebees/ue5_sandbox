#include "TestStaticTurrets.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestLasers.h"
#include "TestStaticTurretsConfig.h"
#include "TestStaticTurretsProxy.h"

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>

ATestStaticTurrets::ATestStaticTurrets()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    RootComponent->SetMobility(EComponentMobility::Static);
    instances->SetMobility(EComponentMobility::Static);

    instances->SetupAttachment(RootComponent);
}

// Actor life cycle
void ATestStaticTurrets::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!actor_config) {
        return;
    }

    configure_ismc();
}
void ATestStaticTurrets::BeginPlay() {
    Super::BeginPlay();

    if (actor_config == nullptr) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestStaticTurrets: actor_config is nullptr."));
        SetActorTickEnabled(false);
        return;
    }

    configure_ismc();
}
void ATestStaticTurrets::Tick(float dt) {
    Super::Tick(dt);
}

void ATestStaticTurrets::configure_ismc() {
    RootComponent->SetMobility(EComponentMobility::Static);

    instances->SetCanEverAffectNavigation(false);
    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    instances->SetStaticMesh(actor_config->mesh);
}

// Spawning
void ATestStaticTurrets::spawn_instance(FTransform const& transform) {
    instances->AddInstance(transform, true);
}
