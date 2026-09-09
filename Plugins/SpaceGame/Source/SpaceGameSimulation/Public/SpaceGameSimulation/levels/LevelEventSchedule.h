#pragma once

#include <SpaceGameSimulation/levels/LevelDefinitionSoA.h>
#include <SpaceGameSimulation/levels/LevelEventGroupCounts.h>
#include <SpaceGameSimulation/levels/LevelMissionEvents.h>

namespace ml {
struct SPACEGAMESIMULATION_API FLevelEventSchedule {
    FLevelSpawnGroups spawn_groups{};
    FLevelCapitalSpawnEvents capital_spawns{};
    FLevelTurretSpawnEvents turret_spawns{};
    FLevelMissionEvents mission_events{};
    TArray<uint64> execution_ticks{};
    TArray<FLevelEventGroupCounts> event_group_counts{};

    auto add_spawn_group(ETestEntityType type, int32 offset, int32 count) -> bool;
    auto add_mission_group(ELevelMissionEventType type, TConstArrayView<int32> values) -> bool;
};
}
