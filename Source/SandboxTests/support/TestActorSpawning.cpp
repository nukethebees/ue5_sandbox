#include "TestActorSpawning.h"

#include <SandboxTests/SandboxTestLogCategories.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>

#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

namespace ioj {
auto spawn_player_ship(UWorld& world,
                       TSubclassOf<ATestSpaceShip> const player_class,
                       FPlayerShipConfig const* const player_config,
                       FTransform const& transform) -> ATestSpaceShip* {
    if (!IsValid(player_class)) {
        UE_LOG(LogSandboxTest,
               Warning,
               TEXT("Cannot spawn player ship: Player ship class is invalid"));
        return nullptr;
    }
    if (!player_config) {
        UE_LOG(LogSandboxTest,
               Warning,
               TEXT("Cannot spawn player ship: Player ship config is invalid"));
        return nullptr;
    }

    auto* const player_ship{world.SpawnActorDeferred<ATestSpaceShip>(player_class, transform)};
    if (!IsValid(player_ship)) {
        UE_LOG(LogSandboxTest, Warning, TEXT("Cannot spawn player ship: Deferred spawn failed"));
        return nullptr;
    }

    player_ship->set_actor_config(player_config);
    auto* const finished_actor{UGameplayStatics::FinishSpawningActor(player_ship, transform)};
    if (!IsValid(finished_actor)) {
        UE_LOG(LogSandboxTest, Warning, TEXT("Cannot spawn player ship: Finish spawning failed"));
        return nullptr;
    }

    return player_ship;
}

}
