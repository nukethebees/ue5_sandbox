#pragma once

#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/simulation/collision_uniform_grid.h>
#include <SpaceGame/simulation/EntityAABBs.h>

struct FTestEntityRegistry;

namespace ml::ioj {
struct SPACEGAME_API FCollisionSystem {
  public:
    FCollisionSystem() = default;

    void initialise(FEntityAABBs const& bounds);
    void update();

    auto get_entity_aabbs() const noexcept -> FEntityAABBs const& { return entity_aabbs_; }
    auto get_uniform_grid() noexcept -> CollisionUniformGrid& { return uniform_grid_; }
    auto get_uniform_grid() const noexcept -> CollisionUniformGrid const& { return uniform_grid_; }

    void set_entity_registry(FTestEntityRegistry const& registry);
  private:
    void rebuild_grid();

    FTestEntityRegistry const* entity_registry_{nullptr};
    CollisionUniformGrid uniform_grid_{};

    FEntityAABBs entity_aabbs_{};
};
}
