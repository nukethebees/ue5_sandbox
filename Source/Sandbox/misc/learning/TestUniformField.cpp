#include "TestUniformField.h"

ATestUniformField::ATestUniformField() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    // Run when everyone else is finished
    PrimaryActorTick.TickGroup = ETickingGroup::TG_PostPhysics;
}

void ATestUniformField::BeginPlay() {
    Super::BeginPlay();
}
void ATestUniformField::Tick(float dt) {
    Super::Tick(dt);

    update_cells();
}
void ATestUniformField::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}

void ATestUniformField::add_source(FTestUniformFieldPointSource const& source) {
    point_sources.Add(source);
}
void ATestUniformField::update_cells() {
    point_sources.Reset();
}
