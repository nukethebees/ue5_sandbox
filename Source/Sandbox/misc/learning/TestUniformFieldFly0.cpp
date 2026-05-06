#include "TestUniformFieldFly0.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "TestUniformField.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ATestUniformFieldFly0::ATestUniformFieldFly0()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestUniformFieldFly0::Tick(float dt) {
    Super::Tick(dt);
}
void ATestUniformFieldFly0::BeginPlay() {
    Super::BeginPlay();
}
void ATestUniformFieldFly0::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
