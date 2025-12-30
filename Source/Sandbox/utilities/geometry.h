#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

namespace ml {
// Get angle (rads) between two lines from the same point
// Written for vision calculations
inline auto get_angle_between_norm_lines(FVector a, FVector b) -> double {
    return FMath::Acos(FVector::DotProduct(a, b));
}

// 2 segments = 3 lines
auto subdivide_arc_into_segments(float starting_angle_deg, float arc_deg, int32 segments)
    -> TArray<float>;
}
