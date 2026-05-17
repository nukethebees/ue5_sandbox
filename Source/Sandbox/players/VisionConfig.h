#pragma once

#include <CoreMinimal.h>

#include "VisionConfig.generated.h"

USTRUCT()
struct FVisionConfig {
    GENERATED_BODY()

    FVisionConfig() = default;
    FVisionConfig(float radius, float half_angle_deg)
        : radius(radius)
        , half_angle_deg(half_angle_deg) {}

    auto half_angle_degrees() const -> float;
    auto half_angle_rad() const -> float;

    UPROPERTY(EditAnywhere)
    float radius{1000.f};
    UPROPERTY(EditAnywhere)
    float half_angle_deg{45.f};
};
