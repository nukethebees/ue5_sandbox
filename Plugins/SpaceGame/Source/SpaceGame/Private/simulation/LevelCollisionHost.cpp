#include "SpaceGame/simulation/LevelCollisionHost.h"

#include <ioj/sim/entities/entity_type.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/container_ops.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/strings.h>

#include <ActorEditorUtils.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/PrimitiveComponent.h>
#include <Engine/EngineTypes.h>
#include <Engine/Level.h>
#include <Engine/StaticMesh.h>
#include <GameFramework/Actor.h>
#include <Math/Transform.h>
#include <PhysicsEngine/AggregateGeom.h>
#include <PhysicsEngine/BodySetup.h>

#include <algorithm>
#include <array>

namespace ioj {
namespace {
using EntityAABBs = sim::collision::EntityAABBs;

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

void clear_aabb(EntityAABBs& aabbs, sim::EntityType const type) {
    aabbs.set_centre(type, Vector3f{0.f, 0.f, 0.f});
    aabbs.set_half_extents(type, Vector3f{0.f, 0.f, 0.f});
}

void set_mesh_aabb(EntityAABBs& aabbs,
                   sim::EntityType const type,
                   TCHAR const* const entity_name,
                   UStaticMesh const* const mesh,
                   FLevelStartErrors& errors) {
    clear_aabb(aabbs, type);

    if (!IsValid(mesh)) {
        return;
    }

    auto const aabb{get_aabb(*mesh)};
    if (!aabb.IsValid) {
        errors.add(FString::Printf(
            TEXT("Cannot initialise collision bounds for %s: mesh %s has no query-enabled "
                 "simple collision geometry"),
            entity_name,
            *mesh->GetName()));
        return;
    }

    FVector3f const centre{aabb.GetCenter()};
    FVector3f const half_extents{aabb.GetExtent()};

    aabbs.set_centre(type, Vector3f{centre.X, centre.Y, centre.Z});
    aabbs.set_half_extents(type, Vector3f{half_extents.X, half_extents.Y, half_extents.Z});
}

#if WITH_EDITOR
auto has_unsupported_geometry(FKAggregateGeom const& geometry) -> bool {
    return any_non_empty(geometry.TaperedCapsuleElems,
                         geometry.LevelSetElems,
                         geometry.SkinnedLevelSetElems,
                         geometry.MLLevelSetElems,
                         geometry.SkinnedTriangleMeshElems);
}

#endif
}

auto FLevelCollisionHost::extract_entity_bounds(EntityMeshes const& meshes)
    -> FEntityBoundsExtractionResult {
    FEntityBoundsExtractionResult result{std::in_place};
    auto& bounds{result.value()};
    FLevelStartErrors errors;
    for (auto const entity_type : EnumTraits<sim::EntityType>::values) {
        auto const entity_name{to_fstring(sim::to_string(entity_type))};
        set_mesh_aabb(bounds, entity_type, *entity_name, meshes[entity_type], errors);
    }
    if (errors.has_errors()) {
        return FEntityBoundsExtractionResult{std::unexpect, MoveTemp(errors)};
    }
    return result;
}
#if WITH_EDITOR
auto FLevelCollisionHost::exports_actor(AActor const& actor, FCollisionGridConfig const& config)
    -> bool {
    return !FActorEditorUtils::IsABuilderBrush(&actor) &&
           !actor_is_any(actor, config.omitted_collision_actor_classes) &&
           actor_is_any(actor, config.harvested_collision_actor_classes);
}

auto FLevelCollisionHost::export_static_collision(ULevel const& level,
                                                  FCollisionGridConfig const& config)
    -> std::expected<std::vector<WorldAABB>, FString> {
    std::vector<WorldAABB> boxes;
    for (auto const& actor : level.Actors) {
        if (!IsValid(actor) || !exports_actor(*actor, config)) {
            continue;
        }

        TInlineComponentArray<UPrimitiveComponent*> components;
        actor->GetComponents(components);
        for (auto* component : components) {
            if (!IsValid(component) || component->Mobility != EComponentMobility::Static ||
                !component->IsQueryCollisionEnabled()) {
                continue;
            }

            auto const* body{component->GetBodySetup()};
            if (!IsValid(body)) {
                continue;
            }

            if (component->IsA<UInstancedStaticMeshComponent>() ||
                body->CollisionTraceFlag == CTF_UseComplexAsSimple ||
                has_unsupported_geometry(body->AggGeom)) {
                return std::unexpected{FString::Printf(TEXT("Unsupported static collision on %s"),
                                                       *component->GetPathName())};
            }

            auto const bounds{get_aabb(body->AggGeom, component->GetComponentTransform())};
            if (!bounds.IsValid) {
                continue;
            }

            FVector3f const min{bounds.Min};
            FVector3f const max{bounds.Max};
            if (min.ContainsNaN() || max.ContainsNaN()) {
                return std::unexpected{FString::Printf(TEXT("Non-finite static collision on %s"),
                                                       *component->GetPathName())};
            }

            boxes.push_back({{min.X, min.Y, min.Z}, {max.X, max.Y, max.Z}});
        }
    }

    std::ranges::sort(boxes, {}, [](WorldAABB const& box) {
        return std::array{
            box.min.x(), box.min.y(), box.min.z(), box.max.x(), box.max.y(), box.max.z()};
    });
    return boxes;
}
#endif
}
