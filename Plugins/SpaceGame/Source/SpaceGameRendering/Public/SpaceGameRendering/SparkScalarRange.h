#pragma once

#include <CoreMinimal.h>

#include "SparkScalarRange.generated.h"

USTRUCT(BlueprintType)
struct SPACEGAMERENDERING_API FSparkScalarRange {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Sparks")
    float min{0.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    float max{0.0f};
};
