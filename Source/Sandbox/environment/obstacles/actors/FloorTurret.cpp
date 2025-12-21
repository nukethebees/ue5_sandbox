#include "Sandbox/environment/obstacles/actors/FloorTurret.h"

#include "Sandbox/logging/SandboxLogCategories.h"

AFloorTurret::AFloorTurret() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AFloorTurret::Tick(float dt) {
    Super::Tick(dt);

    switch (state) {
        case EFloorTurretState::Disabled: {
            SetActorTickEnabled(false);
            break;
        }
        case EFloorTurretState::Watching: {
            break;
        }
        case EFloorTurretState::Attacking: {
            break;
        }
        default: {
            UE_LOG(LogSandboxActor, Warning, TEXT("Unhandled enum state."));
            break;
        }
    }
}
void AFloorTurret::set_state(EFloorTurretState new_state) {
    state = new_state;
    SetActorTickEnabled(state != EFloorTurretState::Disabled);
}

void AFloorTurret::BeginPlay() {
    Super::BeginPlay();
}
