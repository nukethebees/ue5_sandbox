#include "SpaceShipGameMode.h"

#include "Sandbox/environment/effects/ShipPostProcessing.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/SpaceShip.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

class AShipPostProcessing;

ASpaceShipGameMode::ASpaceShipGameMode() {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ASpaceShipGameMode::InitGame(FString const& MapName,
                                  FString const& Options,
                                  FString& ErrorMessage) {
    Super::InitGame(MapName, Options, ErrorMessage);
}

void ASpaceShipGameMode::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(world, GetWorld());

    int pps{0};
    for (auto* actor : TActorRange<AShipPostProcessing>(world)) {
        pps++;
    }

    if (pps != 1) {
        UE_LOG(LogSandboxGameMode, Warning, TEXT("%d post processing volumes found."), pps);
    }
}
