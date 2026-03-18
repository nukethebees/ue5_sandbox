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

    using PlayerPawn = ASpaceShip;

    TRY_INIT_PTR(pc, UGameplayStatics::GetPlayerController(this, 0));
    TRY_INIT_PTR(player_actor, UGameplayStatics::GetActorOfClass(this, PlayerPawn::StaticClass()));
    TRY_INIT_PTR(player_pawn, Cast<PlayerPawn>(player_actor));
    pc->Possess(player_pawn);

    TRY_INIT_PTR(world, GetWorld());

    int pps{0};
    for (auto* actor : TActorRange<AShipPostProcessing>(world)) {
        pps++;
    }

    if (pps != 1) {
        UE_LOG(LogSandboxCore, Warning, TEXT("%d post processing volumes found."), pps);
    }
}
void ASpaceShipGameMode::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
