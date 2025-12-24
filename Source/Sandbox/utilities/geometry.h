#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

namespace ml {
// Get angle (rads) between two lines from the same point
// Written for vision calculations
inline auto get_angle_between_lines(FVector a, FVector b) -> double {
    return FMath::Acos(FVector::DotProduct(a, b));
}
}
