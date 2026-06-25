#pragma once

#include "BarrelRoll.generated.h"

USTRUCT()
struct FBarrelRollConfig {
    GENERATED_BODY()

    FBarrelRollConfig() = default;

    UPROPERTY(EditAnywhere)
    float roll_speed{1080.f};
    UPROPERTY(EditAnywhere)
    float roll_duration{0.66f};
    UPROPERTY(EditAnywhere)
    float roll_cooldown{0.33f};
};

USTRUCT()
struct FBarrelRollState {
    GENERATED_BODY()

    FBarrelRollState() = default;

    auto is_rolling() const { return roll_time_remaining > 0.f; }
    auto can_start_rolling() const { return !is_rolling() && (cooldown_remaining <= 0.f); }

    UPROPERTY(VisibleAnywhere)
    float direction{0.f};
    UPROPERTY(VisibleAnywhere)
    float roll_time_remaining{0.f};
    UPROPERTY(EditAnywhere)
    float cooldown_remaining{0.5f};
};

USTRUCT(BlueprintType)
struct FBarrelRoll {
    GENERATED_BODY()

    FBarrelRoll() = default;

    auto is_rolling() const { return time_remaining > 0.f; }
    auto can_roll() const { return time_remaining < (-1.f * roll_cooldown); }

    UPROPERTY(EditAnywhere)
    float roll_speed{1080.f};
    UPROPERTY(EditAnywhere)
    float roll_duration{2.5f};
    UPROPERTY(EditAnywhere)
    float roll_cooldown{0.5f};

    UPROPERTY(VisibleAnywhere)
    float direction{0.f};
    UPROPERTY(VisibleAnywhere)
    float time_remaining{0.f};
};
