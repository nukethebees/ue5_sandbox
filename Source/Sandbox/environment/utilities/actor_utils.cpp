#include "Sandbox/environment/utilities/actor_utils.h"

#include "Sandbox/logging/SandboxLogCategories.h"

namespace ml {
auto get_actor_corners(AActor const& actor) -> FActorCorners {
    FVector origin;
    FVector box_extent;
    constexpr bool only_colliding_components{false};
    constexpr bool include_from_child_actors{true};
    actor.GetActorBounds(only_colliding_components, origin, box_extent, include_from_child_actors);

    FActorCorners out{{origin + FVector(+box_extent.X, +box_extent.Y, +box_extent.Z),
                       origin + FVector(+box_extent.X, +box_extent.Y, -box_extent.Z),
                       origin + FVector(+box_extent.X, -box_extent.Y, +box_extent.Z),
                       origin + FVector(+box_extent.X, -box_extent.Y, -box_extent.Z),
                       origin + FVector(-box_extent.X, +box_extent.Y, +box_extent.Z),
                       origin + FVector(-box_extent.X, +box_extent.Y, -box_extent.Z),
                       origin + FVector(-box_extent.X, -box_extent.Y, +box_extent.Z),
                       origin + FVector(-box_extent.X, -box_extent.Y, -box_extent.Z)}};

    return out;
}
}
