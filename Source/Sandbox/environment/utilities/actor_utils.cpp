#include "Sandbox/environment/utilities/actor_utils.h"

#include "Components/StaticMeshComponent.h"
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
auto get_static_meshes_bounding_box(AActor const& actor) -> FBox {
    FBox box{ForceInit};
    TArray<UStaticMeshComponent*> mesh_comps;
    actor.GetComponents<UStaticMeshComponent>(mesh_comps);

    for (auto const* comp : mesh_comps) {
        if (comp && comp->IsRegistered()) {
            box += comp->Bounds.GetBox();
        }
    }

    return box;
}
}
