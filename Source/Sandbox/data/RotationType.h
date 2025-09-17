#pragma once

#include "CoreMinimal.h"
#include "RotationType.generated.h"

UENUM(BlueprintType)
enum class ERotationType : uint8 {
    DYNAMIC UMETA(DisplayName = "Dynamic"),
    STATIC UMETA(DisplayName = "Static")
};
