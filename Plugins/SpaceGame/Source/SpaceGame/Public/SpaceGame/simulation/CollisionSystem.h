#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SGCollision/world_aabbs.h>
#include <SpaceGame/simulation/EntityAABBs.h>

#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

class UStaticMesh;
struct FTestEntityRegistry;

namespace ml::ioj {
struct CollisionUniformGrid {
    static inline FVector3f const origin{FVector3f::ZeroVector};

    TArray<int32> cell_entity_offsets;
    TArray<int32> cell_entity_lengths;
    TArray<FRegistryEntityHandle> entities;
    WorldAABBs aabbs;
};

struct SPACEGAME_API FCollisionSystem {
  public:
    using EntityMeshes = TStaticArray<UStaticMesh const*, FEntityAABBs::num_rows>;

    FCollisionSystem() = default;
    explicit FCollisionSystem(FTestEntityRegistry const& entity_registry) noexcept;

    void initialise(EntityMeshes const& meshes);
    void update();

    auto get_entity_aabbs() const noexcept -> FEntityAABBs const& { return entity_aabbs_; }

    auto get_grid_dims() const noexcept -> FIntVector3 { return grid_dims_; }
    void set_grid_dims(FIntVector3 const grid_dims) noexcept { grid_dims_ = grid_dims; }

    auto get_cell_dims() const noexcept -> FVector3f { return cell_dims_; }
    void set_cell_dims(FVector3f const cell_dims) noexcept { cell_dims_ = cell_dims; }
  private:
    auto to_cell_x(float value) const -> int32;
    auto to_cell_y(float value) const -> int32;
    auto to_cell_z(float value) const -> int32;

    void rebuild_grid();

    FTestEntityRegistry const* entity_registry_{nullptr};
    CollisionUniformGrid uniform_grid_{};

    FEntityAABBs entity_aabbs_{};
    FIntVector3 grid_dims_{FIntVector3::ZeroValue};
    FVector3f cell_dims_{FVector3f::ZeroVector};
};
}
