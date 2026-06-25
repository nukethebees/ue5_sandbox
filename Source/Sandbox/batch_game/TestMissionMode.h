#pragma once

#include <CoreMinimal.h>

#include "TestMissionMode.generated.h"

UENUM()
enum class ETestMissionMode : uint8 {
    None,
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
};
