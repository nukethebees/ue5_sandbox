#pragma once

#include <CoreMinimal.h>

namespace ml {
using FLevelEventCount = uint8;

struct SPACEGAMESIMULATION_API FLevelEventGroupCounts {
    FLevelEventCount spawn_groups{};
    FLevelEventCount mission_groups{};
};
}
