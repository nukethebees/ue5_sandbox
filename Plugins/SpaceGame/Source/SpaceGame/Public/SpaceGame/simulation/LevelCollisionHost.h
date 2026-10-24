#pragma once
#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/collision/entity_aabbs.h>
#include <ioj/sim/collision/world_aabb.h>
#include <ioj/sim/entities/entity_type.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <cstddef>
#include <expected>
#include <vector>

class UStaticMesh;
class UWorld;
struct FCollisionGridConfig;

namespace ioj {
using FEntityBoundsExtractionResult = std::expected<sim::collision::EntityAABBs, FLevelStartErrors>;

struct SPACEGAME_API FLevelCollisionHost {
    using EntityMeshes = EnumArray<sim::EntityType, UStaticMesh const*>;
    static auto extract_entity_bounds(EntityMeshes const& meshes) -> FEntityBoundsExtractionResult;
    static auto extract_static_geometry(UWorld& world,
                                        FCollisionGridConfig const& config,
                                        sim::collision::GridGeometry grid_geometry)
        -> std::vector<sim::collision::WorldAABB>;
};
}
