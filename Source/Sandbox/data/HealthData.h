#pragma once

#include "CoreMinimal.h"

#include "HealthData.generated.h"

USTRUCT(BlueprintType)
struct FHealthData {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float current{0.0f};

    UPROPERTY(BlueprintReadOnly)
    float max{100.0f};

    UPROPERTY(BlueprintReadOnly)
    float percent{0.0f};
};