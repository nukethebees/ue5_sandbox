#pragma once

#include <SpaceGame/levels/LevelDefinitionSoA.h>

namespace ml {
struct SPACEGAME_API FLevelMissionEvents {
    FLevelMissionEventGroups groups{};
    TArray<int32> values{};
};
}
