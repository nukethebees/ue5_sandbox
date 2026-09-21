#pragma once
#include <ioj/sim/collision_grid.h>
#include <ioj/sim/entity_aabbs.h>
#include <ioj/sim/entity_type.h>
#include <ioj/sim/world_aabbs.h>
#include <SpaceGame/simulation/StaticCollisionSources.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <cstddef>
#include <expected>

class UStaticMesh;
class UWorld;
struct FCollisionGridConfig;

namespace ml::ioj {
using FEntityBoundsExtractionResult =
    std::expected<::ioj::sim::collision::EntityAABBs, FLevelStartErrors>;

struct SPACEGAME_API FLevelCollisionHost {
    using EntityMeshes = ml::EnumArray<::ioj::sim::EntityType,
                                       UStaticMesh const*,
                                       static_cast<std::size_t>(::ioj::sim::EntityType::COUNT)>;
    static auto extract_entity_bounds(EntityMeshes const& meshes) -> FEntityBoundsExtractionResult;
    auto initialise_static_geometry(UWorld& world,
                                    FCollisionGridConfig const& config,
                                    ::ioj::sim::collision::GridGeometry grid_geometry)
        -> ::ioj::sim::collision::WorldAABBs;
    void restore_collision();
    auto get_static_collision_sources() const noexcept -> FStaticCollisionSources::ConstView {
        return static_collision_sources_.get_const_view();
    }
  private:
    FStaticCollisionSources static_collision_sources_;
};
}
