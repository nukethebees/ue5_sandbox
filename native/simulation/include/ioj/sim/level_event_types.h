#pragma once

#include <cstdint>
#include <limits>

namespace ioj::sim {
using LevelEventCount = std::uint8_t;

inline constexpr LevelEventCount max_level_event_count{std::numeric_limits<LevelEventCount>::max()};

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
