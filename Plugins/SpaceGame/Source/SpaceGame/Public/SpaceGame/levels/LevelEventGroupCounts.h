#pragma once

#include <CoreMinimal.h>

namespace ml {
using FLevelEventCount = uint8;

struct SPACEGAME_API FLevelEventGroupCounts {
    FLevelEventCount spawn_groups{};
    FLevelEventCount mission_groups{};
};

SPACEGAME_API auto to_level_event_count(int32 count) -> FLevelEventCount;
}
