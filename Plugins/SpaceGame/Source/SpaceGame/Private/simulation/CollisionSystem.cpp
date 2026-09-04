#include "SpaceGame/simulation/CollisionSystem.h"

#include <SGCollision/mesh_data_extraction.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/PrimitiveComponent.h>
#include <Engine/StaticMesh.h>
#include <EngineUtils.h>
#include <GameFramework/Actor.h>
#include <PhysicsEngine/BodySetup.h>

namespace ml::ioj {
namespace {
static_assert(FEntityAABBs::space_ship_index == std::to_underlying(ETestEntityType::PlayerShip));
static_assert(FEntityAABBs::static_turret_index == std::to_underlying(ETestEntityType::Turret));
static_assert(FEntityAABBs::capital_ship_index == std::to_underlying(ETestEntityType::CapitalShip));
static_assert(FEntityAABBs::fighter_index ==
              std::to_underlying(ETestEntityType::CapitalShipFighter));
static_assert(FEntityAABBs::tube_spinner_index == std::to_underlying(ETestEntityType::TubeSpinner));
static_assert(FEntityAABBs::num_rows == std::to_underlying(ETestEntityType::COUNT));

void clear_aabb(FEntityAABBs& aabbs, int32 const index) {
    aabbs.centre_xs[index] = 0.0f;
    aabbs.centre_ys[index] = 0.0f;
    aabbs.centre_zs[index] = 0.0f;
    aabbs.half_extent_xs[index] = 0.0f;
    aabbs.half_extent_ys[index] = 0.0f;
    aabbs.half_extent_zs[index] = 0.0f;
}

void set_mesh_aabb(FEntityAABBs& aabbs,
                   int32 const index,
                   TCHAR const* const entity_name,
                   UStaticMesh const* const mesh) {
    clear_aabb(aabbs, index);

    if (!IsValid(mesh)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("Cannot initialise collision bounds for %s: mesh is unavailable"),
               entity_name);
        return;
    }

    auto const aabb{ml::get_aabb(*mesh)};
    if (!aabb.IsValid) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("Cannot initialise collision bounds for %s: mesh %s has no valid collision "
                    "AABB"),
               entity_name,
               *mesh->GetName());
        return;
    }

    FVector3f const centre{aabb.GetCenter()};
    FVector3f const half_extents{aabb.GetExtent()};

    aabbs.centre_xs[index] = centre.X;
    aabbs.centre_ys[index] = centre.Y;
    aabbs.centre_zs[index] = centre.Z;
    aabbs.half_extent_xs[index] = half_extents.X;
    aabbs.half_extent_ys[index] = half_extents.Y;
    aabbs.half_extent_zs[index] = half_extents.Z;
}

auto matches_any_class(AActor const& actor, TConstArrayView<TSubclassOf<AActor>> const classes)
    -> bool {
    for (auto const actor_class : classes) {
        if (actor.IsA(actor_class)) {
            return true;
        }
    }
    return false;
}

auto has_unsupported_geometry(FKAggregateGeom const& geometry) -> bool {
    return !geometry.TaperedCapsuleElems.IsEmpty() || !geometry.LevelSetElems.IsEmpty() ||
           !geometry.SkinnedLevelSetElems.IsEmpty() || !geometry.MLLevelSetElems.IsEmpty() ||
           !geometry.SkinnedTriangleMeshElems.IsEmpty();
}
}

void FCollisionSystem::initialise(EntityMeshes const& meshes) {
    auto const count{FEntityAABBs::num()};
    for (int32 i{0}; i < count; ++i) {
        auto const entity_type{static_cast<ETestEntityType>(i)};
        set_mesh_aabb(entity_aabbs_, i, LexToString(entity_type), meshes[i]);
    }
}
void FCollisionSystem::initialise_static_geometry(UWorld& world,
                                                  FCollisionGridConfig const& config) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::initialise_static_geometry);
    checkf(config.is_valid(), TEXT("Cannot harvest static collision with an invalid grid config"));

    for (auto const& source : static_collision_sources_) {
        if (auto* const component{source.component.Get()}; IsValid(component)) {
            component->SetCollisionEnabled(source.original_collision_enabled);
        }
    }
    static_collision_sources_.Reset();
    uniform_grid_.set_static_aabbs({});

    WorldAABBs static_aabbs;
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

        if (matches_any_class(*actor, config.omitted_collision_actor_classes)) {
            continue;
        }
        if (!matches_any_class(*actor, config.harvested_collision_actor_classes)) {
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

            if (component->GetOwner() != actor) {
                reject_component(TEXT("component owner does not match the enumerated actor"));
                continue;
            }
            if (component->Mobility != EComponentMobility::Static) {
                reject_component(TEXT("component mobility is not static"));
                continue;
            }
            if (!component->IsQueryCollisionEnabled()) {
                reject_component(TEXT("component has no query collision"));
                continue;
            }
            if (component->IsA<UInstancedStaticMeshComponent>()) {
                reject_component(TEXT("instanced static mesh components are not supported"));
                continue;
            }

            auto* const body_setup{component->GetBodySetup()};
            if (!IsValid(body_setup)) {
                reject_component(TEXT("component has no body setup"));
                continue;
            }
            if (body_setup->CollisionTraceFlag == CTF_UseComplexAsSimple) {
                reject_component(TEXT("complex-as-simple collision is not supported"));
                continue;
            }
            if (has_unsupported_geometry(body_setup->AggGeom)) {
                reject_component(TEXT("aggregate geometry contains unsupported shape types"));
                continue;
            }

            auto const aabb{ml::get_aabb(body_setup->AggGeom, component->GetComponentTransform())};
            if (!aabb.IsValid) {
                reject_component(TEXT("component has no query-enabled simple collision geometry"));
                continue;
            }

            FVector3f const min_point{aabb.Min};
            FVector3f const max_point{aabb.Max};
            auto const min_coord{uniform_grid_.to_min_cell_coord(min_point)};
            auto const max_coord{uniform_grid_.to_max_cell_coord(max_point)};
            if (min_point.ContainsNaN() || max_point.ContainsNaN() ||
                !uniform_grid_.is_cell_coord_in_bounds(min_coord) ||
                !uniform_grid_.is_cell_coord_in_bounds(max_coord)) {
                reject_component(TEXT("world AABB is invalid or outside the collision grid"));
                continue;
            }

            static_aabbs.mins.add(min_point);
            static_aabbs.maxes.add(max_point);
            static_collision_sources_.Add({actor,
                                           component,
                                           component->GetComponentTransform(),
                                           component->GetCollisionEnabled()});
        }
    }

    uniform_grid_.set_static_aabbs(MoveTemp(static_aabbs));
    for (auto const& source : static_collision_sources_) {
        if (auto* const component{source.component.Get()}; IsValid(component)) {
            component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    UE_LOG(LogSandbox,
           Display,
           TEXT("Harvested %d static collision components; reported %d unsupported components and "
                "%d unexpected actors"),
           static_collision_sources_.Num(),
           unsupported_component_count,
           unexpected_actor_count);
}
void FCollisionSystem::update() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::update);
    rebuild_grid();
}
void FCollisionSystem::set_entity_registry(FTestEntityRegistry const& registry) {
    entity_registry_ = &registry;
    uniform_grid_.set_entity_registry(registry);
}
void FCollisionSystem::rebuild_grid() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::rebuild_grid);
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
}
