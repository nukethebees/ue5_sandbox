#include "TestStaticTurrets.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestStaticTurretsConfig.h"
#include "TestStaticTurretsProxy.h"

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>

ATestStaticTurrets::ATestStaticTurrets()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);
}

// Actor life cycle
void ATestStaticTurrets::Tick(float dt) {
    Super::Tick(dt);
}
void ATestStaticTurrets::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!turrets_config) {
        return;
    }

    configure_ismc();
}
void ATestStaticTurrets::BeginPlay() {
    Super::BeginPlay();
}

void ATestStaticTurrets::configure_ismc() {
    instances->SetCanEverAffectNavigation(false);
    instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    instances->SetMobility(EComponentMobility::Static);

    instances->SetStaticMesh(turrets_config->mesh);
}
