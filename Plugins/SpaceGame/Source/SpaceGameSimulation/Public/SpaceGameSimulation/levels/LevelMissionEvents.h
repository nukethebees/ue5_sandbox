#pragma once

#include <SpaceGameSimulation/levels/LevelDefinitionSoA.h>

namespace ml {
struct SPACEGAMESIMULATION_API FLevelMissionEvents {
    FLevelMissionEventGroups groups{};
    TArray<int32> values{};
};
}
