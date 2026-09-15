#pragma once
#include <ioj/sim/collision/collision_system.h>
#include <SandboxCore/enum_array.h>
#include <SpaceGame/simulation/StaticCollisionSources.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>
#include <SpaceGameSimulation/simulation/EntityAABBs.h>

#include <expected>
#include <optional>

class UStaticMesh;
class UWorld;
struct FCollisionGridConfig;

namespace ml::ioj {
using FEntityBoundsExtractionResult = std::expected<FEntityAABBs, FLevelStartErrors>;

struct SPACEGAME_API FLevelCollisionHost {
    using EntityMeshes = TEnumArray<ETestEntityType, UStaticMesh const*>;
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
