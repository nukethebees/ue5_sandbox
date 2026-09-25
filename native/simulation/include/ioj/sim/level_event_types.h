#pragma once

#include "ioj/sim/level_event_values.h"

#include <cstdint>
#include <limits>

namespace ioj::sim {
inline constexpr LevelEventCount max_level_event_count{std::numeric_limits<LevelEventCount>::max()};

struct LevelEventGroupCounts {
    LevelEventCount spawn_groups{};
    LevelEventCount mission_groups{};
};

[[nodiscard]] auto try_to_level_event_count(std::int32_t count, LevelEventCount& result) noexcept
    -> bool;
}
