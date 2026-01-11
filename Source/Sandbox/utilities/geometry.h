#pragma once

#include "CoreMinimal.h"
#include "Math/Box.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"

#include "Sandbox/environment/data/ActorCorners.h"

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

inline auto get_box_corners(FVector origin, FVector box_extent) -> FActorCorners {
    return {{origin + FVector(+box_extent.X, +box_extent.Y, +box_extent.Z),
             origin + FVector(+box_extent.X, +box_extent.Y, -box_extent.Z),
             origin + FVector(+box_extent.X, -box_extent.Y, +box_extent.Z),
             origin + FVector(+box_extent.X, -box_extent.Y, -box_extent.Z),
             origin + FVector(-box_extent.X, +box_extent.Y, +box_extent.Z),
             origin + FVector(-box_extent.X, +box_extent.Y, -box_extent.Z),
             origin + FVector(-box_extent.X, -box_extent.Y, +box_extent.Z),
             origin + FVector(-box_extent.X, -box_extent.Y, -box_extent.Z)}};
}
inline auto get_box_corners(FBox box) -> FActorCorners {
    return get_box_corners(box.GetCenter(), box.GetExtent());
}

}
