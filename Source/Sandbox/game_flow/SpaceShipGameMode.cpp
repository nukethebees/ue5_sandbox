#include "SpaceShipGameMode.h"

#include "Sandbox/players/SpaceShip.h"

#include "Kismet/GameplayStatics.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

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
}
