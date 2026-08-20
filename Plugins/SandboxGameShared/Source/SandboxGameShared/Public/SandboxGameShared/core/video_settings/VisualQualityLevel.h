#pragma once

#include "CoreMinimal.h"

enum class EVisualQualityLevel : int32 {
    Auto = -1,
    Low = 0,
    Medium = 1,
    High = 2,
    Epic = 3,
    Cinematic = 4
};

SANDBOXGAMESHARED_API FText const& GetQualityLevelDisplayName(EVisualQualityLevel level);
