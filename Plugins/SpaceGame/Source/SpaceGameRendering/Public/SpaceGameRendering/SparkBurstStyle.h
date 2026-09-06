#pragma once

#include <CoreMinimal.h>
#include <SpaceGameRendering/SparkScalarRange.h>

#include "SparkBurstStyle.generated.h"

USTRUCT(BlueprintType)
struct SPACEGAMERENDERING_API FSparkBurstStyle {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0"))
    int32 count{16};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    FSparkScalarRange speed{3000.0f, 10000.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    FSparkScalarRange lifetime{0.08f, 0.25f};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    FSparkScalarRange size{4.0f, 12.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0"))
    float intensity{20.0f};

    UPROPERTY(EditAnywhere,
              Category = "Sparks",
              meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
    float spread_angle_degrees{90.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0", Units = "s"))
    float streak_time{0.025f};
};
