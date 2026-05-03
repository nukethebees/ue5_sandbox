#include "TestUniformFieldSource.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ATestUniformFieldPointSource::ATestUniformFieldPointSource()
    : point_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    point_mesh->SetupAttachment(RootComponent);
}

void ATestUniformFieldPointSource::Tick(float dt) {
    Super::Tick(dt);
}
void ATestUniformFieldPointSource::BeginPlay() {
    Super::BeginPlay();
}

void ATestUniformFieldPointSource::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
