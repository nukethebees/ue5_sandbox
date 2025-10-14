#pragma once

#include "CoreMinimal.h"

#include "SimpleAIState.generated.h"

UENUM(BlueprintType)
enum class ESimpleAIState : uint8 {
    Wandering UMETA(DisplayName = "Wandering"),
    Following UMETA(DisplayName = "Following"),
    Investigating UMETA(DisplayName = "Investigating")
};
