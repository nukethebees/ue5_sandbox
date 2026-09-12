#pragma once

#include <cstdint>

namespace ml::simulation {
using LevelEventCount = std::uint8_t;

struct LevelEventGroupCounts {
    LevelEventCount spawn_groups{};
    LevelEventCount mission_groups{};
};

enum class LevelMissionEventType : std::uint8_t {
    MustSurvive,
    RequiredKill,
    IncreaseKillTarget,
};
}
