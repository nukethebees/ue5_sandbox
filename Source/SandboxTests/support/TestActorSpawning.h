#pragma once

#include <SpaceGame/ships/player/TestSpaceShip.h>

#include <CoreMinimal.h>

struct FPlayerShipConfig;
class UWorld;

namespace ioj {
auto spawn_player_ship(UWorld& world,
                       TSubclassOf<ATestSpaceShip> const player_class,
                       FPlayerShipConfig const* player_config,
                       FTransform const& transform = FTransform::Identity) -> ATestSpaceShip*;

}
