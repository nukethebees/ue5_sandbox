#pragma once

#include <sandbox/simulation/attack_distance_band.h>

#include <CoreMinimal.h>

#include "AttackDistanceBand.generated.h"

USTRUCT(BlueprintType)
struct FAttackDistanceBand {
    GENERATED_BODY()

    bool values_are_valid() const { return to_native().values_are_valid(); }

    auto to_native() const noexcept -> ml::simulation::AttackDistanceBand {
        return {
            .minimum_ratio = minimum_ratio,
            .desired_ratio = desired_ratio,
            .maximum_ratio = maximum_ratio,
        };
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
