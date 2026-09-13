#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <sandbox/simulation/levels/LevelEventSchedule.h>
#include <span>
#include <vector>

namespace ml {
auto FLevelEventSchedule::add_spawn_group(ml::simulation::EntityType const type,
                                          std::int32_t const offset,
                                          std::int32_t const count) -> bool {
    if (count == 0) {
        return true;
    }
    assert(!event_group_counts.empty());
    assert(execution_ticks.size() == event_group_counts.size());

    FLevelEventCount payload_count;
    auto& tick_counts{event_group_counts.back()};
    FLevelEventCount group_count;
    if (!simulation::try_to_level_event_count(count, payload_count) ||
        !simulation::try_to_level_event_count(tick_counts.spawn_groups + 1, group_count)) {
        return false;
    }

    tick_counts.spawn_groups = group_count;
    spawn_groups.add(type, offset, payload_count);
    return true;
}

auto FLevelEventSchedule::add_mission_group(ml::simulation::LevelMissionEventType const type,
                                            std::span<std::int32_t const> const values) -> bool {
    if (values.size() > std::numeric_limits<FLevelEventCount>::max()) {
        return false;
    }
    auto const count{static_cast<std::int32_t>(values.size())};
    if (count == 0) {
        return true;
    }
    assert(!event_group_counts.empty());
    assert(execution_ticks.size() == event_group_counts.size());

    FLevelEventCount payload_count;
    auto& tick_counts{event_group_counts.back()};
    FLevelEventCount group_count;
    if (!simulation::try_to_level_event_count(count, payload_count) ||
        !simulation::try_to_level_event_count(tick_counts.mission_groups + 1, group_count)) {
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
