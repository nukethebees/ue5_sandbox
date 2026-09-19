#include <cassert>
#include <cstdint>
#include <ioj/sim/levels/level_event_schedule.h>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
auto LevelEventSchedule::add_spawn_group(EntityType const type,
                                         std::int32_t const offset,
                                         std::int32_t const count) -> bool {
    if (count == 0) {
        return true;
    }
    assert(!event_group_counts.empty());
    assert(execution_ticks.size() == event_group_counts.size());

    LevelEventCount payload_count;
    auto& tick_counts{event_group_counts.back()};
    LevelEventCount group_count;
    if (!try_to_level_event_count(count, payload_count) ||
        !try_to_level_event_count(tick_counts.spawn_groups + 1, group_count)) {
        return false;
    }

    tick_counts.spawn_groups = group_count;
    spawn_groups.add(type, offset, payload_count);
    return true;
}

auto LevelEventSchedule::add_mission_group(LevelMissionEventType const type,
                                           std::span<std::int32_t const> const values) -> bool {
    if (values.size() > max_level_event_count) {
        return false;
    }
    auto const count{static_cast<std::int32_t>(values.size())};
    if (count == 0) {
        return true;
    }
    assert(!event_group_counts.empty());
    assert(execution_ticks.size() == event_group_counts.size());

    LevelEventCount payload_count;
    auto& tick_counts{event_group_counts.back()};
    LevelEventCount group_count;
    if (!try_to_level_event_count(count, payload_count) ||
        !try_to_level_event_count(tick_counts.mission_groups + 1, group_count)) {
        return false;
    }

    tick_counts.mission_groups = group_count;
    auto& groups{mission_events.groups};
    assert(mission_events.values.size() <=
           static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()));
    groups.add(type, static_cast<std::int32_t>(mission_events.values.size()), payload_count);
    mission_events.values.insert(mission_events.values.end(), values.begin(), values.end());
    return true;
}

}
