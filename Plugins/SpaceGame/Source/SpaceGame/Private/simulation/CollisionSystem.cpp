#include "SpaceGame/simulation/CollisionSystem.h"

#include <SGCollision/mesh_data_extraction.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <Engine/StaticMesh.h>

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
}

void FCollisionSystem::initialise(EntityMeshes const& meshes) {
    auto const count{FEntityAABBs::num()};
    for (int32 i{0}; i < count; ++i) {
        auto const entity_type{static_cast<ETestEntityType>(i)};
        set_mesh_aabb(entity_aabbs_, i, LexToString(entity_type), meshes[i]);
    }
}
void FCollisionSystem::update() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::update);
    rebuild_grid();
}
void FCollisionSystem::set_entity_registry(FTestEntityRegistry const& registry) {
    entity_registry_ = &registry;
}
void FCollisionSystem::rebuild_grid() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::rebuild_grid);

    if (entity_registry_ == nullptr) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Cannot rebuild the collision grid: entity registry is unavailable"));
        return;
    }

    uniform_grid_.rebuild_grid(*entity_registry_, entity_aabbs_);
}
}
