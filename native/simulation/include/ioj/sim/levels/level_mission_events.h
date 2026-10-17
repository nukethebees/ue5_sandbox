#pragma once
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_mission_event_groups.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>

#include <cstdint>
#include <vector>

namespace ioj::sim {
struct LevelMissionEvents {
    LevelMissionEventGroups groups{};
    std::vector<std::int32_t> values{};
};
}
