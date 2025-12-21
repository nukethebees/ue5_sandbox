#include "Sandbox/environment/obstacles/actors/FloorTurret.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/logging/SandboxLogCategories.h"

AFloorTurret::AFloorTurret()
    : base_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBase"))}
    , cannon_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretCannon"))}
    , muzzle_point{CreateDefaultSubobject<UArrowComponent>(TEXT("TurretCannon"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    base_mesh->SetupAttachment(RootComponent);
    cannon_mesh->SetupAttachment(base_mesh);
    muzzle_point->SetupAttachment(cannon_mesh);
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

void AFloorTurret::scan_for_targets() {}
