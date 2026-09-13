#pragma once
#include <cstdint>
#include <vector>

#include <sandbox/simulation/levels/LevelRuntimeEvents.h>

namespace ml {
struct FLevelMissionEvents {
    FLevelMissionEventGroups groups{};
    std::vector<std::int32_t> values{};
};
}
