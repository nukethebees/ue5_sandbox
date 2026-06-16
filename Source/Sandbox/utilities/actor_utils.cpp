#include "Sandbox/utilities/actor_utils.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/geometry.h"

#include <Kismet/KismetMathLibrary.h>
#include "Components/StaticMeshComponent.h"

namespace ml {
auto get_best_display_name(AActor const& actor) -> FString {
    // GetActorLabel can return incorrect values for a CDO
    if (actor.HasAnyFlags(RF_ClassDefaultObject)) { return actor.GetClass()->GetName(); }

#if WITH_EDITOR
    return actor.GetActorLabel();
#else
    return actor.GetName();
#endif
}

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
        if (comp && comp->IsRegistered()) { box += comp->Bounds.GetBox(); }
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

void fatal_if_actor_transform_not_identity(AActor const& actor) {
    if (actor.GetActorTransform().Equals(FTransform::Identity)) { return; }

    UE_LOG(LogSandboxActor,
           Fatal,
           TEXT("%s must have an identity transform"),
           *get_best_display_name(actor));
}

void fatal_if_actor_root_not_static(AActor const& actor) {
    auto const* root{actor.GetRootComponent()};

    if (root == nullptr) {
        UE_LOG(LogSandbox, Fatal, TEXT("%s has no root component"), *actor.GetName());
    }

    if (root->Mobility != EComponentMobility::Static) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("%s root component is not static"),
               *ml::get_best_display_name(actor));
    }
}

void set_actor_component_mobility(AActor& actor, EComponentMobility::Type mobility) {
    TInlineComponentArray<USceneComponent*> components;
    actor.GetComponents(components);

    for (auto* const component : components) {
        if (component != nullptr) { component->SetMobility(mobility); }
    }
}
}
