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

namespace ml {
constexpr bool is_valid(ETestTeam const team) {
    using T = uint8;
    return static_cast<T>(team) < static_cast<T>(ETestTeam::COUNT);
}
}
