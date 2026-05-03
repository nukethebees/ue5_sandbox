#include "TestUniformFieldSource.h"

#include "Components/StaticMeshComponent.h"

ATestUniformFieldPointSource::ATestUniformFieldPointSource()
    : point_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {}

void ATestUniformFieldPointSource::Tick(float dt) {
    Super::Tick(dt);
}
void ATestUniformFieldPointSource::BeginPlay() {
    Super::BeginPlay();
}

void ATestUniformFieldPointSource::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
