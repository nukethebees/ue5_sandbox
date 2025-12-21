#include "Sandbox/environment/obstacles/actors/FloorTurret.h"

AFloorTurret::AFloorTurret() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AFloorTurret::Tick(float dt) {
    Super::Tick(dt);
}

void AFloorTurret::BeginPlay() {
    Super::BeginPlay();
}
