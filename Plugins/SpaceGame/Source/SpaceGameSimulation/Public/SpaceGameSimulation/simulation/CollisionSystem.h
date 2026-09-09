#pragma once

#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/collision_uniform_grid.h>
#include <SpaceGameSimulation/simulation/EntityAABBs.h>

struct FTestEntityRegistry;

namespace ml::ioj {
struct SPACEGAMESIMULATION_API FCollisionSystem {
  public:
    explicit FCollisionSystem(FTestEntityRegistry const& registry) noexcept;
    FCollisionSystem(FCollisionSystem const&) = delete;
    FCollisionSystem(FCollisionSystem&&) = delete;
    auto operator=(FCollisionSystem const&) -> FCollisionSystem& = delete;
    auto operator=(FCollisionSystem&&) -> FCollisionSystem& = delete;

    void initialise(FEntityAABBs const& bounds);
    void update();

    auto get_entity_aabbs() const noexcept -> FEntityAABBs const& { return entity_aabbs_; }
    auto get_uniform_grid() noexcept -> CollisionUniformGrid& { return uniform_grid_; }
    auto get_uniform_grid() const noexcept -> CollisionUniformGrid const& { return uniform_grid_; }
  private:
    void rebuild_grid();

    CollisionUniformGrid uniform_grid_;

    FEntityAABBs entity_aabbs_{};
};
}
