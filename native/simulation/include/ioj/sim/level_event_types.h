#pragma once

#include <cstdint>

namespace ioj::sim {
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

[[nodiscard]] auto try_to_level_event_count(std::int32_t count, LevelEventCount& result) noexcept
    -> bool;
}
