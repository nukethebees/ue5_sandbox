#pragma once

#include <CoreMinimal.h>

#include "TestMissionState.generated.h"

UENUM()
enum class ETestMissionState : uint8 {
    NotStarted,
    Running,
    Succeeded,
    Failed,
    Disabled,
};
