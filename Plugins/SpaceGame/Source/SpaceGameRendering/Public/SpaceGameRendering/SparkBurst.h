#pragma once

#include <CoreMinimal.h>
#include <SpaceGameRendering/SparkScalarRange.h>

struct SPACEGAMERENDERING_API FSparkBurst {
    FVector3f location{FVector3f::ZeroVector};
    FVector3f direction{FVector3f::UpVector};
    FLinearColor colour{FLinearColor::White};
    FSparkScalarRange speed;
    FSparkScalarRange lifetime;
    FSparkScalarRange size;
    float intensity{1.0f};
    float spread_angle_degrees{90.0f};
    float streak_time{0.0f};
    int32 count{0};
    uint32 seed{0};
};
