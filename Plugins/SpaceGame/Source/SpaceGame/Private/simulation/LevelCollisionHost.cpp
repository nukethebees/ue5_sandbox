#include "SpaceGame/simulation/LevelCollisionHost.h"

#include <ioj/sim/entity_type.h>
#include <ioj/sim/world_aabb_operations.h>
#include <ioj/sim/world_aabbs.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/container_ops.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/strings.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/PrimitiveComponent.h>
#include <Engine/EngineTypes.h>
#include <Engine/StaticMesh.h>
#include <EngineUtils.h>
#include <GameFramework/Actor.h>
#include <Math/Transform.h>
#include <PhysicsEngine/AggregateGeom.h>
#include <PhysicsEngine/BodySetup.h>

namespace ml::ioj {
namespace {
using EntityAABBs = ::ioj::sim::collision::EntityAABBs;

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

void clear_aabb(EntityAABBs& aabbs, ::ioj::sim::EntityType const type) {
    aabbs.set_centre(type, ml::make_vector3f(0.f, 0.f, 0.f));
    aabbs.set_half_extents(type, ml::make_vector3f(0.f, 0.f, 0.f));
}

void set_mesh_aabb(EntityAABBs& aabbs,
                   ::ioj::sim::EntityType const type,
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

    aabbs.set_centre(type, ml::make_vector3f(centre.X, centre.Y, centre.Z));
    aabbs.set_half_extents(type, ml::make_vector3f(half_extents.X, half_extents.Y, half_extents.Z));
}

auto has_unsupported_geometry(FKAggregateGeom const& geometry) -> bool {
    return ml::any_non_empty(geometry.TaperedCapsuleElems,
                             geometry.LevelSetElems,
                             geometry.SkinnedLevelSetElems,
                             geometry.MLLevelSetElems,
                             geometry.SkinnedTriangleMeshElems);
}

struct FStaticCollisionComponentData {
    FVector3f min_point{FVector3f::ZeroVector};
    FVector3f max_point{FVector3f::ZeroVector};
    ECollisionEnabled::Type original_collision_mode{ECollisionEnabled::NoCollision};
};

auto extract_static_collision_component(UPrimitiveComponent& component,
                                        ::ioj::sim::collision::GridGeometry const grid_geometry,
                                        AActor const* const expected_owner,
                                        TCHAR const*& rejection_reason)
    -> TOptional<FStaticCollisionComponentData> {
    auto* const actor{component.GetOwner()};
    if (!IsValid(actor)) {
        rejection_reason = TEXT("component has no valid owner");
        return {};
    }
    if (expected_owner && actor != expected_owner) {
        rejection_reason = TEXT("component owner does not match the enumerated actor");
        return {};
    }
    if (component.GetCollisionEnabled() == ECollisionEnabled::NoCollision ||
        !component.IsQueryCollisionEnabled()) {
        rejection_reason = TEXT("component has no query collision");
        return {};
    }
    if (component.Mobility != EComponentMobility::Static) {
        rejection_reason = TEXT("component mobility is not static");
        return {};
    }
    if (component.IsA<UInstancedStaticMeshComponent>()) {
        rejection_reason = TEXT("instanced static mesh components are not supported");
        return {};
    }

    auto* const body_setup{component.GetBodySetup()};
    if (!IsValid(body_setup)) {
        rejection_reason = TEXT("component has no body setup");
        return {};
    }
    if (body_setup->CollisionTraceFlag == CTF_UseComplexAsSimple) {
        rejection_reason = TEXT("complex-as-simple collision is not supported");
        return {};
    }
    if (has_unsupported_geometry(body_setup->AggGeom)) {
        rejection_reason = TEXT("aggregate geometry contains unsupported shape types");
        return {};
    }

    auto const aabb{get_aabb(body_setup->AggGeom, component.GetComponentTransform())};
    if (!aabb.IsValid) {
        rejection_reason = TEXT("component has no query-enabled simple collision geometry");
        return {};
    }

    FVector3f const min_point{aabb.Min};
    FVector3f const max_point{aabb.Max};
    auto const [min_coord, max_coord]{::ioj::sim::collision::to_cell_coord_bounds(
        grid_geometry,
        ml::make_vector3f(min_point.X, min_point.Y, min_point.Z),
        ml::make_vector3f(max_point.X, max_point.Y, max_point.Z))};
    if (min_point.ContainsNaN() || max_point.ContainsNaN() ||
        !::ioj::sim::collision::is_cell_coord_in_bounds(grid_geometry, min_coord) ||
        !::ioj::sim::collision::is_cell_coord_in_bounds(grid_geometry, max_coord)) {
        rejection_reason = TEXT("world AABB is invalid or outside the collision grid");
        return {};
    }

    return FStaticCollisionComponentData{
        .min_point = min_point,
        .max_point = max_point,
        .original_collision_mode = component.GetCollisionEnabled(),
    };
}
}

auto FLevelCollisionHost::extract_entity_bounds(EntityMeshes const& meshes)
    -> FEntityBoundsExtractionResult {
    FEntityBoundsExtractionResult result{std::in_place};
    auto& bounds{result.value()};
    FLevelStartErrors errors;
    for (auto const entity_type : ml::EnumTraits<::ioj::sim::EntityType>::values) {
        auto const entity_name{ml::to_fstring(::ioj::sim::to_string(entity_type))};
        set_mesh_aabb(bounds, entity_type, *entity_name, meshes[entity_type], errors);
    }
    if (errors.has_errors()) {
        return FEntityBoundsExtractionResult{std::unexpect, MoveTemp(errors)};
    }
    return result;
}
auto FLevelCollisionHost::initialise_static_geometry(UWorld& world,
                                                     FCollisionGridConfig const& config)
    -> ::ioj::sim::collision::WorldAABBs {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::ioj::sim::FLevelCollisionHost::initialise_static_geometry);
    checkf(config.is_valid(), TEXT("Cannot harvest static collision with an invalid grid config"));
    auto const dimensions{config.calculate_grid_dimensions()};
    auto const grid_geometry{::ioj::sim::collision::GridGeometry{
        {dimensions.X, dimensions.Y, dimensions.Z},
        ml::make_vector3f(config.cell_size.X, config.cell_size.Y, config.cell_size.Z)}};

    auto const previous_sources{static_collision_sources_.get_const_view()};
    auto const previous_source_count{previous_sources.num()};
    for (int32 source_index{}; source_index < previous_source_count; ++source_index) {
        if (auto* const component{previous_sources.components[source_index].Get()};
            IsValid(component)) {
            component->SetCollisionEnabled(previous_sources.original_collision_modes[source_index]);
        }
    }
    static_collision_sources_.reset();

    ::ioj::sim::collision::WorldAABBs static_aabbs;
    int32 unexpected_actor_count{};
    int32 unsupported_component_count{};

    for (TActorIterator<AActor> actor_it{&world}; actor_it; ++actor_it) {
        auto* const actor{*actor_it};
        if (!IsValid(actor)) {
            continue;
        }

        TInlineComponentArray<UPrimitiveComponent*> components;
        actor->GetComponents(components);

        auto const has_collision{components.ContainsByPredicate([](auto const* const component) {
            return IsValid(component) &&
                   component->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
        })};
        if (!has_collision) {
            continue;
        }

        if (ml::actor_is_any(*actor, config.omitted_collision_actor_classes)) {
            continue;
        }
        if (!ml::actor_is_any(*actor, config.harvested_collision_actor_classes)) {
            ++unexpected_actor_count;
            UE_LOG(LogSandbox,
                   Warning,
                   TEXT("Collision-enabled actor %s of class %s is neither harvested nor omitted"),
                   *actor->GetPathName(),
                   *actor->GetClass()->GetPathName());
            continue;
        }

        for (auto* const component : components) {
            if (!IsValid(component) ||
                component->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
                continue;
            }

            auto const reject_component{[&](TCHAR const* const reason) {
                ++unsupported_component_count;
                UE_LOG(LogSandbox,
                       Warning,
                       TEXT("Cannot harvest collision component %s on actor %s: %s"),
                       *component->GetPathName(),
                       *actor->GetPathName(),
                       reason);
            }};

            TCHAR const* rejection_reason{};
            auto const data{extract_static_collision_component(
                *component, grid_geometry, actor, rejection_reason)};
            if (!data) {
                reject_component(rejection_reason);
                continue;
            }

            ::ioj::sim::collision::add(static_aabbs,
                                       {data->min_point.X, data->min_point.Y, data->min_point.Z},
                                       {data->max_point.X, data->max_point.Y, data->max_point.Z});
            static_collision_sources_.add(component, data->original_collision_mode);
        }
    }

    auto const sources{static_collision_sources_.get_const_view()};
    auto const source_count{sources.num()};
    for (int32 source_index{}; source_index < source_count; ++source_index) {
        if (auto* const component{sources.components[source_index].Get()}; IsValid(component)) {
            component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    UE_LOG(LogSandbox,
           Display,
           TEXT("Harvested %d static collision components; reported %d unsupported components and "
                "%d unexpected actors"),
           static_collision_sources_.num(),
           unsupported_component_count,
           unexpected_actor_count);
    return static_aabbs;
}
void FLevelCollisionHost::restore_collision() {
    auto const sources{static_collision_sources_.get_const_view()};
    auto const count{sources.num()};
    for (int32 i{}; i < count; ++i) {
        if (auto* component{sources.components[i].Get()}; IsValid(component)) {
            component->SetCollisionEnabled(sources.original_collision_modes[i]);
        }
    }
    static_collision_sources_.reset();
}
}
