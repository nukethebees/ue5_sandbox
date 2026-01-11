#include "Sandbox/environment/utilities/actor_utils.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/geometry.h"

namespace ml {
auto get_actor_corners(AActor const& actor) -> FActorCorners {
    FVector origin;
    FVector box_extent;
    constexpr bool only_colliding_components{true};
    constexpr bool include_from_child_actors{true};
    actor.GetActorBounds(only_colliding_components, origin, box_extent, include_from_child_actors);

    return get_box_corners(origin, box_extent);
}
}
