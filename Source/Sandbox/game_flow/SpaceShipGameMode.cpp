#include "SpaceShipGameMode.h"

#include "Sandbox/environment/effects/ShipPostProcessing.h"
#include "SandboxGameShared/utilities/macros/null_checks.hpp"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/UnrealEndpoint.h>

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace ioj {
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

void ASpaceShipGameMode::StartPlay() {
    Super::StartPlay();
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    TActorIterator<ATestBatchOrchestrator> actor{GetWorld()};
    if (actor) {
        actor->report_world_ready();
    } else {
        unreal_endpoint().level_initialized("Level world has no orchestrator");
    }
#endif
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

} // namespace ioj
