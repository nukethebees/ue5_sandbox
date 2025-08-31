#pragma once

#include <compare>

#include "SpeedBoost.generated.h"

USTRUCT(BlueprintType)
struct FSpeedBoost {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float multiplier{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float duration{0.0f};

    auto operator<=>(FSpeedBoost const&) const = default;
};
