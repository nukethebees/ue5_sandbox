#pragma once
#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/entity_aabbs.h>
#include <ioj/sim/entity_type.h>
#include <SpaceGame/simulation/StaticCollisionSources.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <array>
#include <expected>
#include <optional>

class UStaticMesh;
class UWorld;
struct FCollisionGridConfig;

namespace ml::ioj {
using FEntityBoundsExtractionResult =
    std::expected<::ioj::sim::collision::EntityAABBs, FLevelStartErrors>;

struct SPACEGAME_API FLevelCollisionHost {
    using EntityMeshes =
        std::array<UStaticMesh const*, ::ioj::sim::collision::EntityAABBs::num_rows>;
    static auto extract_entity_bounds(EntityMeshes const& meshes) -> FEntityBoundsExtractionResult;
    auto initialise_static_geometry(UWorld& world,
                                    FCollisionGridConfig const& config,
                                    ::ioj::sim::collision::CollisionUniformGrid const& grid)
        -> ::ioj::sim::collision::WorldAABBs;
    auto add_static_geometry(UPrimitiveComponent& component,
                             ::ioj::sim::collision::CollisionUniformGrid const& grid)
        -> std::optional<::ioj::sim::collision::WorldAABB>;
    void restore_collision();
    auto get_static_collision_sources() const noexcept -> FStaticCollisionSources::ConstView {
        return static_collision_sources_.get_const_view();
    }
  private:
    FStaticCollisionSources static_collision_sources_;
};
}
