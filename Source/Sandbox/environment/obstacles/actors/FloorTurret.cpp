#include "Sandbox/environment/obstacles/actors/FloorTurret.h"

AFloorTurret::AFloorTurret() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AFloorTurret::Tick(float dt) {
    Super::Tick(dt);
}
void AFloorTurret::set_state(EFloorTurretState new_state) {
    state = new_state;
    SetActorTickEnabled(state != EFloorTurretState::Disabled);
}

void AFloorTurret::BeginPlay() {
    Super::BeginPlay();
}
