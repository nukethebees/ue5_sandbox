#pragma once

#include "CoreMinimal.h"

#include "TestTeam.generated.h"

UENUM()
enum class ETestTeam : uint8 {
    White, // Neutral
    Red,
    Green,
    Blue,
    Orange,
    Yellow,
    COUNT UMETA(DisplayName = "Count", Hidden),
};
