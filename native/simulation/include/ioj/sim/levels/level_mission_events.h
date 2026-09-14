#pragma once
#include <cstdint>
#include <vector>

#include <ioj/sim/levels/level_runtime_events.h>

namespace ioj::sim {
struct LevelMissionEvents {
    LevelMissionEventGroups groups{};
    std::vector<std::int32_t> values{};
};
}
