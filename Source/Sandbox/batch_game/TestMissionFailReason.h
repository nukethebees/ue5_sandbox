#pragma once

#include <CoreMinimal.h>

#include "TestMissionFailReason.generated.h"

UENUM()
enum class ETestMissionFailReason : uint8 {
    None,
    PlayerKilled,
    TimeElapsed,
};
