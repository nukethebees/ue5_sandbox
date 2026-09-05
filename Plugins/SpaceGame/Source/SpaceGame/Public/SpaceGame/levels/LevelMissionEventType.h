#pragma once

#include <CoreMinimal.h>

namespace ml {
enum class ELevelMissionEventType : uint8 {
    MustSurvive,
    RequiredKill,
    IncreaseKillTarget,
};
}
