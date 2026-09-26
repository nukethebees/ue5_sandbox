#pragma once
#include <ioj/sim/levels/level_runtime_events.h>

#include <cstdint>
#include <vector>

namespace ioj::sim {
struct LevelMissionEvents {
    LevelMissionEventGroups groups{};
    std::vector<std::int32_t> values{};
};
}
