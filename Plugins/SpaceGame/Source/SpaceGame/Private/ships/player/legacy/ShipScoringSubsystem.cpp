#include "SpaceGame/ships/player/legacy/ShipScoringSubsystem.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include "SpaceGame/ships/player/legacy/SpaceShip.h"

void UShipScoringSubsystem::register_kills(FShipAttackResult attack) {
    auto const kills{attack.killed_actors.Num()};

    UE_LOG(LogSandboxSubsystem, Verbose, TEXT("Registering %d kill(s)."), kills);
    if (attack.instigator.IsValid()) {
        if (auto* ship{Cast<ASpaceShip>(attack.instigator.Get())}) {
            ship->record_kills(kills);
        } else {
            UE_LOG(LogSandboxSubsystem, Verbose, TEXT("Instigator not a ship"));
        }
    } else {
        UE_LOG(LogSandboxSubsystem, Warning, TEXT("Ship pointer no longer valid"));
    }
}
void UShipScoringSubsystem::register_points(ASpaceShip& ship, int32 pts) {
    ship.record_kills(pts);
}
