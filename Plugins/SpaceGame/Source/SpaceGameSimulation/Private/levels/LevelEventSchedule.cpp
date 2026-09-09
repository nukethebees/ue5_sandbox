#include <SpaceGameSimulation/levels/LevelEventSchedule.h>

namespace ml {
auto try_to_level_event_count(int32 const count, FLevelEventCount& result) -> bool {
    if (count < 0 || count > TNumericLimits<FLevelEventCount>::Max()) {
        return false;
    }
    result = static_cast<FLevelEventCount>(count);
    return true;
}

auto FLevelEventSchedule::add_spawn_group(ETestEntityType const type,
                                          int32 const offset,
                                          int32 const count) -> bool {
    if (count == 0) {
        return true;
    }
    check(!event_group_counts.IsEmpty());
    check(execution_ticks.Num() == event_group_counts.Num());

    FLevelEventCount payload_count;
    auto& tick_counts{event_group_counts.Last()};
    FLevelEventCount group_count;
    if (!try_to_level_event_count(count, payload_count) ||
        !try_to_level_event_count(tick_counts.spawn_groups + 1, group_count)) {
        return false;
    }

    tick_counts.spawn_groups = group_count;
    spawn_groups.add(type, offset, payload_count);
    return true;
}

auto FLevelEventSchedule::add_mission_group(ELevelMissionEventType const type,
                                            TConstArrayView<int32> const values) -> bool {
    auto const count{values.Num()};
    if (count == 0) {
        return true;
    }
    check(!event_group_counts.IsEmpty());
    check(execution_ticks.Num() == event_group_counts.Num());

    FLevelEventCount payload_count;
    auto& tick_counts{event_group_counts.Last()};
    FLevelEventCount group_count;
    if (!try_to_level_event_count(count, payload_count) ||
        !try_to_level_event_count(tick_counts.mission_groups + 1, group_count)) {
        return false;
    }

    tick_counts.mission_groups = group_count;
    auto& groups{mission_events.groups};
    groups.add(type, mission_events.values.Num(), payload_count);
    mission_events.values.Append(values.GetData(), count);
    return true;
}

}
