#pragma once
#include <CoreMinimal.h>
#include "LevelSimulationState.generated.h"

UENUM(BlueprintType)
enum class EOrchestratorState : uint8 {
    Uninitialised,
    Paused,
    Running,
    Stopped,
};
