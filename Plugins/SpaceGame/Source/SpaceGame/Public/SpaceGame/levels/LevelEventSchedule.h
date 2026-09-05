#pragma once

#include <SpaceGame/levels/LevelDefinitionSoA.h>
#include <SpaceGame/levels/LevelEventGroupCounts.h>
#include <SpaceGame/levels/LevelMissionEvents.h>

namespace ml {
struct SPACEGAME_API FLevelEventSchedule {
    FLevelSpawnGroups spawn_groups{};
    FLevelCapitalSpawnEvents capital_spawns{};
    FLevelTurretSpawnEvents turret_spawns{};
    FLevelMissionEvents mission_events{};
    TArray<uint64> execution_ticks{};
    TArray<FLevelEventGroupCounts> event_group_counts{};

    void add_spawn_group(ETestEntityType type, int32 offset, int32 count);
    void add_mission_group(ELevelMissionEventType type, TConstArrayView<int32> values);
};
}
