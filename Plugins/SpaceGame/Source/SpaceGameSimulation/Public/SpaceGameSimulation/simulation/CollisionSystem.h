#pragma once

#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/collision_uniform_grid.h>
#include <SpaceGameSimulation/simulation/EntityAABBs.h>

#include <compare>

struct FTestEntityRegistry;

namespace ml::ioj {
struct FEntityOverlapPair {
    FRegistryEntityHandle first;
    FRegistryEntityHandle second;

    auto operator<=>(FEntityOverlapPair const&) const noexcept = default;
};

struct SPACEGAMESIMULATION_API FCollisionSystem {
  public:
    explicit FCollisionSystem(FTestEntityRegistry const& registry) noexcept;
    FCollisionSystem(FCollisionSystem const&) = delete;
    FCollisionSystem(FCollisionSystem&&) = delete;
    auto operator=(FCollisionSystem const&) -> FCollisionSystem& = delete;
    auto operator=(FCollisionSystem&&) -> FCollisionSystem& = delete;

    void initialise(FEntityAABBs const& bounds);
    void update(TConstArrayView<FRegistryEntityHandle> collision_dirty_entities);

    auto get_entity_aabbs() const noexcept -> FEntityAABBs const& { return entity_aabbs_; }
    auto get_overlap_pairs() const noexcept -> TConstArrayView<FEntityOverlapPair> {
        return overlap_pairs_;
    }
    auto get_uniform_grid() noexcept -> CollisionUniformGrid& { return uniform_grid_; }
    auto get_uniform_grid() const noexcept -> CollisionUniformGrid const& { return uniform_grid_; }
  private:
    void rebuild_grid();
    void find_overlap_pairs(TConstArrayView<FRegistryEntityHandle> collision_dirty_entities);

    FTestEntityRegistry const& entity_registry_;
    CollisionUniformGrid uniform_grid_;

    FEntityAABBs entity_aabbs_{};
    TArray<FEntityOverlapPair> overlap_pairs_;
    TArray<FRegistryEntityHandle> overlap_query_scratch_;
};
}
