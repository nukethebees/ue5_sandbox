#pragma once

#include "CoreMinimal.h"

#include "DefaultAIState.generated.h"

UENUM(BlueprintType)
enum class EDefaultAIState : uint8 {
    Idle UMETA(DisplayName = "Idle"),
    RandomlyMove UMETA(DisplayName = "RandomlyMove"),
    Patrol UMETA(DisplayName = "Patrol"),
};
