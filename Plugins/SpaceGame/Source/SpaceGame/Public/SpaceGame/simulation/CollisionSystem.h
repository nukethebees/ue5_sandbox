#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SGCollision/world_aabbs.h>
#include <SpaceGame/simulation/EntityAABBs.h>
#include <SpaceGame/simulation/EntityCellData.h>

#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

class UStaticMesh;
struct FTestEntityRegistry;

namespace ml::ioj {
struct SPACEGAME_API CollisionUniformGrid {
    static inline FVector3f const origin{FVector3f::ZeroVector};

    auto get_grid_dims() const noexcept -> FIntVector3;
    void set_grid_dims(FIntVector3 const grid_dims) noexcept;

    auto get_cell_dims() const noexcept -> FVector3f;
    void set_cell_dims(FVector3f const cell_dims) noexcept;

    auto num_cells() const -> int32;
    auto get_cell_entities(FIntVector3 const cell_coord) const
        -> TConstArrayView<FRegistryEntityHandle>;

    auto to_min_cell_coord(FVector3f pos) const -> FIntVector3;
    auto to_max_cell_coord(FVector3f pos) const -> FIntVector3;
    auto is_cell_coord_in_bounds(FIntVector3 coord) const -> bool;
    static auto to_string(FIntVector3 value) -> FString;

    void reset();
    void rebuild_grid(FTestEntityRegistry const& entity_registry, FEntityAABBs const& entity_aabbs);
  private:
    auto to_cell_x(float value) const -> int32;
    auto to_cell_y(float value) const -> int32;
    auto to_cell_z(float value) const -> int32;
    auto to_index(int32 x, int32 y, int32 z) const -> int32;
    auto to_index(FIntVector3 coord) const -> int32;
    auto to_index(FVector3f pos) const -> int32;

    FIntVector3 grid_dims_{FIntVector3::ZeroValue};
    FVector3f cell_dims_{FVector3f::ZeroVector};

    TArray<int32> cell_entity_offsets_;
    TArray<int32> cell_entity_counts_;
    TArray<int32> cell_entity_write_indexes_;
    TArray<FRegistryEntityHandle> entities_;
    WorldAABBs aabbs_;

    FEntityCellData entities_buffer_;
};

struct SPACEGAME_API FCollisionSystem {
  public:
    using EntityMeshes = TStaticArray<UStaticMesh const*, FEntityAABBs::num_rows>;

    FCollisionSystem() = default;

    void initialise(EntityMeshes const& meshes);
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
