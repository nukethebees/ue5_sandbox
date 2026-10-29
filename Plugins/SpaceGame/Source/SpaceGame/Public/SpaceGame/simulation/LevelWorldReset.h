#pragma once

#include <UObject/ObjectPtr.h>

class AActor;
class ATestSpaceShip;
class UWorld;
struct FLevelMissionDefinition;
struct FPlayerShipConfig;

namespace ioj {

SPACEGAME_API void reset_level_world(UWorld& world,
                                     AActor const& retained_actor,
                                     TObjectPtr<ATestSpaceShip>& player,
                                     FLevelMissionDefinition& mission,
                                     FPlayerShipConfig const* player_config);

}
