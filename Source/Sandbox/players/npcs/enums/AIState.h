#pragma once

#include "CoreMinimal.h"

#include "AIState.generated.h"

UENUM(BlueprintType)
enum class EAIState : uint8 {
    Idle UMETA(DisplayName = "Idle"),
    RandomlyMove UMETA(DisplayName = "RandomlyMove"),
    Patrol UMETA(DisplayName = "Patrol"),
};
