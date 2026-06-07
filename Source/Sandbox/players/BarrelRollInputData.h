#pragma once

#include "CoreMinimal.h"

#include "BarrelRollInputData.generated.h"

USTRUCT(BlueprintType)
struct FBarrelRollInputData {
    GENERATED_BODY()

    FBarrelRollInputData() = default;

    UPROPERTY(EditAnywhere)
    float input_strength_threshold{0.7f};
    UPROPERTY(EditAnywhere)
    float input_time_threshold{0.25f};
    UPROPERTY(VisibleAnywhere)
    bool threshold_crossed_this_input{false};
    UPROPERTY(VisibleAnywhere)
    float last_crossing_time{0.f};
};
