#pragma once
#include <SpaceGame/simulation/CollisionSystem.h>
#include <SpaceGame/simulation/StaticCollisionSources.h>

class UStaticMesh;
class UWorld;
struct FCollisionGridConfig;

namespace ml::ioj {
struct SPACEGAME_API FLevelCollisionHost {
    using EntityMeshes = TEnumArray<ETestEntityType, UStaticMesh const*>;
    static auto extract_entity_bounds(EntityMeshes const& meshes) -> FEntityAABBs;
    void initialise_static_geometry(UWorld& world,
                                    FCollisionGridConfig const& config,
                                    FCollisionSystem& collision);
    auto add_static_geometry(UPrimitiveComponent& component, FCollisionSystem& collision) -> bool;
    void restore_collision();
    auto get_static_collision_sources() const noexcept -> FStaticCollisionSources::ConstView {
        return static_collision_sources_.get_const_view();
    }
  private:
    FStaticCollisionSources static_collision_sources_;
};
}
