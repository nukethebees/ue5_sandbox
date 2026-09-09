#pragma once

#include <CoreMinimal.h>

#include "AttackDistanceBand.generated.h"

USTRUCT(BlueprintType)
struct FAttackDistanceBand {
    GENERATED_BODY()

    bool values_are_valid() const {
        return minimum_ratio >= 0.f && minimum_ratio <= desired_ratio &&
               desired_ratio <= maximum_ratio && maximum_ratio <= 1.f;
    }

    UPROPERTY(EditAnywhere,
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float minimum_ratio{0.4f};

    UPROPERTY(EditAnywhere,
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float desired_ratio{0.5f};

    UPROPERTY(EditAnywhere,
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float maximum_ratio{0.6f};
};
