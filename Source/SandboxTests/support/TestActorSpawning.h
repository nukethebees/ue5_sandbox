#pragma once

#include <SpaceGame/ships/player/TestSpaceShip.h>

#include <CoreMinimal.h>

class UWorld;

namespace ioj {
struct FPlayerShipConfig;

auto spawn_player_ship(UWorld& world,
                       TSubclassOf<ATestSpaceShip> const player_class,
                       FPlayerShipConfig const* player_config,
                       FTransform const& transform = FTransform::Identity) -> ATestSpaceShip*;

}
