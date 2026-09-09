#pragma once
#include <HAL/Platform.h>
namespace ml {
enum class ELevelMissionMode : uint8 {
    Unspecified,
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
};
}
