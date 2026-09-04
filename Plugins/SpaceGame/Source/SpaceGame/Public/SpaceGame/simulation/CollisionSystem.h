#pragma once

#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/simulation/collision_uniform_grid.h>
#include <SpaceGame/simulation/EntityAABBs.h>
#include <SpaceGame/simulation/StaticCollisionSources.h>

class UStaticMesh;
class UPrimitiveComponent;
class UWorld;
struct FTestEntityRegistry;
struct FCollisionGridConfig;

namespace ml::ioj {
struct SPACEGAME_API FCollisionSystem {
  public:
    using EntityMeshes = TEnumArray<ETestEntityType, UStaticMesh const*>;

    FCollisionSystem() = default;

    void initialise(EntityMeshes const& meshes);
    void initialise_static_geometry(UWorld& world, FCollisionGridConfig const& config);
    auto add_static_geometry(UPrimitiveComponent& component) -> bool;
    void update();

    auto get_entity_aabbs() const noexcept -> FEntityAABBs const& { return entity_aabbs_; }
    auto get_uniform_grid() noexcept -> CollisionUniformGrid& { return uniform_grid_; }
    auto get_uniform_grid() const noexcept -> CollisionUniformGrid const& { return uniform_grid_; }
    auto get_static_collision_sources() const noexcept -> FStaticCollisionSources::ConstView {
        return static_collision_sources_.get_const_view();
    }

    void set_entity_registry(FTestEntityRegistry const& registry);
  private:
    void rebuild_grid();

    FTestEntityRegistry const* entity_registry_{nullptr};
    CollisionUniformGrid uniform_grid_{};

    FEntityAABBs entity_aabbs_{};
    FStaticCollisionSources static_collision_sources_;
};
}
