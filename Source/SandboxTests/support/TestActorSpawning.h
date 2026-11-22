#pragma once

#include <SpaceGame/ships/player/TestSpaceShip.h>

#include <CoreMinimal.h>

class UWorld;

namespace ioj {

auto spawn_player_ship(UWorld& world,
                       TSubclassOf<ATestSpaceShip> const player_class,
                       FTransform const& transform = FTransform::Identity) -> ATestSpaceShip*;

}
