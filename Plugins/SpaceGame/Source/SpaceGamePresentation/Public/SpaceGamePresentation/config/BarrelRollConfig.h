#pragma once

#include "BarrelRollConfig.generated.h"

USTRUCT()
struct FBarrelRollConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    float roll_speed{1080.f};
    UPROPERTY(EditAnywhere)
    float roll_duration{0.66f};
    UPROPERTY(EditAnywhere)
    float roll_cooldown{0.33f};
};
