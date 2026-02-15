#include "Sandbox/players/playable/space_ship/ShipScoringSubsystem.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Sandbox/players/playable/space_ship/SpaceShip.h"

void UShipScoringSubsystem::register_kills(FShipAttackResult attack) {
    int32 kills{attack.killed_actors.Num()};
    if (attack.projectile_type == EShipProjectileType::homing_laser) {
        kills += kills - 1;
    }

    if (attack.instigator.IsValid()) {
        if (auto* ship{Cast<ASpaceShip>(attack.instigator.Get())}) {
            UE_LOG(LogSandboxSubsystem, Verbose, TEXT("Registering %d kill(s) with ship."), kills);
            ship->record_kills(kills);
        }
    } else {
        UE_LOG(LogSandboxSubsystem, Warning, TEXT("Ship pointer no longer valid"));
    }
}
