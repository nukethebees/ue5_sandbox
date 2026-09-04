#include "SGCollision/mesh_data_extraction.h"

#include <Engine/EngineTypes.h>
#include <Engine/StaticMesh.h>
#include <Math/Transform.h>
#include <PhysicsEngine/AggregateGeom.h>
#include <PhysicsEngine/BodySetup.h>

namespace ml {
auto get_aabb(FKAggregateGeom const& geometry, FTransform const& local_to_world) -> FBox {
    auto const scale{local_to_world.GetScale3D()};
    auto world_transform{local_to_world};
    world_transform.RemoveScaling();

    FBox aabb{ForceInit};

    for (auto const& collision_sphere : geometry.SphereElems) {
        if (!CollisionEnabledHasQuery(collision_sphere.GetCollisionEnabled())) {
            continue;
        }

        auto const scaled_sphere{collision_sphere.GetFinalScaled(scale, FTransform::Identity)};
        auto const centre{world_transform.TransformPosition(scaled_sphere.Center)};
        auto const extent{FVector{scaled_sphere.Radius}};
        aabb += FBox{centre - extent, centre + extent};
    }

    for (auto const& collision_box : geometry.BoxElems) {
        if (!CollisionEnabledHasQuery(collision_box.GetCollisionEnabled())) {
            continue;
        }

        auto scaled_box{collision_box.GetFinalScaled(scale, FTransform::Identity)};
        auto box_transform{scaled_box.GetTransform()};
        box_transform.SetScale3D(FVector::OneVector);
        scaled_box.SetTransform(box_transform);
        aabb += scaled_box.CalcAABB(world_transform, 1.f);
    }

    for (auto const& collision_capsule : geometry.SphylElems) {
        if (!CollisionEnabledHasQuery(collision_capsule.GetCollisionEnabled())) {
            continue;
        }

        auto const scaled_capsule{collision_capsule.GetFinalScaled(scale, FTransform::Identity)};
        aabb += scaled_capsule.CalcAABB(world_transform, 1.f);
    }

    for (auto const& collision_convex : geometry.ConvexElems) {
        if (!CollisionEnabledHasQuery(collision_convex.GetCollisionEnabled())) {
            continue;
        }

        aabb += collision_convex.CalcAABB(world_transform, scale);
    }

    return aabb;
}

auto get_aabb(UStaticMesh const& mesh) -> FBox {
    auto const* body_setup{mesh.GetBodySetup()};
    if (body_setup == nullptr) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Cannot extract collision AABB from static mesh %s: mesh has no body setup"),
               *mesh.GetName());
        return FBox{ForceInit};
    }

    return get_aabb(body_setup->AggGeom, FTransform::Identity);
}
}
