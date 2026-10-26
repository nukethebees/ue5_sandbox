#pragma once

#include <ioj/sim/levels/level_event_count.h>

#include <cstdint>
#include <limits>

namespace ioj::sim {
inline constexpr auto max_level_event_count{
    std::numeric_limits<LevelEventCount::value_type>::max()};

struct LevelEventGroupCounts {
    LevelEventCount spawn_groups{};
};

[[nodiscard]] auto try_to_level_event_count(std::int32_t count, LevelEventCount& result) noexcept
    -> bool;
}
