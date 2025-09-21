#pragma once

#include "CoreMinimal.h"

#include "StationStateData.generated.h"

USTRUCT(BlueprintType)
struct FStationStateData {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float remaining_capacity{0.0f};

    UPROPERTY(BlueprintReadOnly)
    float cooldown_remaining{0.0f};

    UPROPERTY(BlueprintReadOnly)
    float cooldown_total{1.0f};

    UPROPERTY(BlueprintReadOnly)
    bool is_ready{false};
};