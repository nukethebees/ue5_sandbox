#pragma once

#include "CoreMinimal.h"

struct alignas(16) FGpuSparkBurst {
    FVector4f location_spawn_time{FVector4f::Zero()};
    FVector4f direction_cone_cosine{FVector4f::Zero()};
    FVector4f colour_intensity{FVector4f::Zero()};
    FVector4f speed_lifetime_ranges{FVector4f::Zero()};
    FVector4f size_streak_reserved{FVector4f::Zero()};
    FUintVector4 allocation{FUintVector4::ZeroValue};
};

static_assert(sizeof(FGpuSparkBurst) == 96);
