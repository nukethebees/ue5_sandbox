#pragma once

#include <ioj/presentation/spark_renderer_settings.h>

#include <CoreMinimal.h>

#include "SparkRendererSettings.generated.h"

namespace ioj {
USTRUCT(BlueprintType)
struct SPACEGAMERENDERING_API FSparkRendererSettings {
    GENERATED_BODY()

    auto to_native() const -> presentation::SparkRendererSettings {
        return {capacity,
                maximum_draw_distance,
                Vector3f{acceleration},
                minimum_thickness_pixels,
                maximum_thickness_pixels,
                maximum_length_pixels};
    }

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "1"))
    int32 capacity{100000};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0", Units = "cm"))
    float maximum_draw_distance{200000.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    FVector3f acceleration{0.0f, 0.0f, -980.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0"))
    float minimum_thickness_pixels{0.75f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0"))
    float maximum_thickness_pixels{12.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0"))
    float maximum_length_pixels{128.0f};
};

} // namespace ioj
