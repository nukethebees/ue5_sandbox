#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

class AActor;

namespace ml {
// Get angle (rads) between two lines from the same point
// Written for vision calculations
inline auto get_angle_between_norm_lines(FVector a, FVector b) -> double {
    return FMath::Acos(FVector::DotProduct(a, b));
}
auto get_norm_vector_to_actor(AActor& actor, FVector point) -> FVector;
auto get_angle_to_actor(AActor& actor, FVector point) -> double;

// 2 segments = 3 lines
auto subdivide_arc_into_segments(float starting_angle_deg, float arc_deg, int32 segments)
    -> TArray<float>;
}
