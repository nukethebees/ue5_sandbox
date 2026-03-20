#pragma once

#include "BarrelRoll.generated.h"

USTRUCT(BlueprintType)
struct FBarrelRoll {
    GENERATED_BODY()

    FBarrelRoll() = default;

    auto is_rolling() const { return time_remaining > 0.f; }
    auto can_roll() const { return time_remaining < (-1.f * roll_cooldown); }

    UPROPERTY(EditAnywhere)
    float roll_speed{90.f};
    UPROPERTY(EditAnywhere)
    float roll_duration{2.5f};
    UPROPERTY(EditAnywhere)
    float roll_cooldown{0.5f};

    UPROPERTY(VisibleAnywhere)
    float direction{0.f};
    UPROPERTY(VisibleAnywhere)
    float time_remaining{0.f};
};
