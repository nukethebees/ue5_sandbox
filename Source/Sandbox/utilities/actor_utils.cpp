#include "Sandbox/utilities/actor_utils.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/geometry.h"

#include <Kismet/KismetMathLibrary.h>
#include "Components/StaticMeshComponent.h"

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
auto actor_is_within(AActor const& actor, AActor const& other, bool only_colliding_components)
    -> bool {
    FVector origin;
    FVector extent;
    other.GetActorBounds(only_colliding_components, origin, extent);

    return UKismetMathLibrary::IsPointInBox(actor.GetActorLocation(), origin, extent);
}
void face_actor(AActor& actor, AActor const& other) {
    return face_point(actor, other.GetActorLocation());
}
void face_point(AActor& actor, FVector const& point) {
    actor.SetActorRotation((point - actor.GetActorLocation()).Rotation());
}
}
