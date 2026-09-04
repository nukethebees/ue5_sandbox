#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SGCollision/world_aabbs.h>
#include <SpaceGame/simulation/EntityAABBs.h>
#include <SpaceGame/simulation/EntityCellData.h>

#include <CoreMinimal.h>

class UStaticMesh;
struct FTestEntityRegistry;

namespace ml {
struct FLineTracesConstView;
struct FTraceHitsView;
}

namespace ml::ioj {
struct FCellCoordBounds {
    FIntVector3 min;
    FIntVector3 max;
};

struct SPACEGAME_API CollisionUniformGrid {
    static inline FVector3f const origin{FVector3f::ZeroVector};

    auto is_configured() const noexcept -> bool;

    auto get_grid_dims() const noexcept -> FIntVector3;
    void set_grid_dims(FIntVector3 const grid_dims) noexcept;

    auto get_cell_dims() const noexcept -> FVector3f;
    void set_cell_dims(FVector3f const cell_dims) noexcept;

    void set_entity_registry(FTestEntityRegistry const& reg) noexcept;

    auto num_cells() const -> int32;
    auto get_cell_entities(FIntVector3 const cell_coord) const
        -> TConstArrayView<FRegistryEntityHandle>;

    auto to_cell_coord(FVector3f pos) const -> FIntVector3;
    auto to_min_cell_coord(FVector3f pos) const -> FIntVector3;
    auto to_max_cell_coord(FVector3f pos) const -> FIntVector3;
    auto to_cell_coord_bounds(FVector3f min_point, FVector3f max_point) const -> FCellCoordBounds;

    auto to_cell_min_x(int32 x) const -> float;
    auto to_cell_min_y(int32 y) const -> float;
    auto to_cell_min_z(int32 z) const -> float;
    auto to_cell_min(int32 x, int32 y, int32 z) const -> FVector3f;
    auto to_cell_min(FIntVector3 coord) const -> FVector3f;

    auto to_cell_centre_x(int32 x) const -> float;
    auto to_cell_centre_y(int32 y) const -> float;
    auto to_cell_centre_z(int32 z) const -> float;
    auto to_cell_centre(int32 x, int32 y, int32 z) const -> FVector3f;
    auto to_cell_centre(FIntVector3 coord) const -> FVector3f;

    auto is_cell_coord_in_bounds(FIntVector3 coord) const -> bool;
    auto is_cell_coord_in_bounds(FIntVector3 min_coord, FIntVector3 max_coord) const -> bool;
    static auto to_string(FIntVector3 value) -> FString;

    void reset();
    void set_static_aabbs(WorldAABBs static_aabbs);
    auto add_static_aabb(FVector3f min_point, FVector3f max_point) -> int32;
    void rebuild_grid(FEntityAABBs const& entity_aabbs);

    auto get_static_aabbs() const noexcept -> WorldAABBs const& { return static_aabbs_; }

    void trace_aabbs(FLineTracesConstView const& traces,
                     FTraceHitsView const& hits,
                     TConstArrayView<FRegistryEntityHandle> ignored_entities = {}) const;
  private:
    auto to_cell_x(float value) const -> int32;
    auto to_cell_y(float value) const -> int32;
    auto to_cell_z(float value) const -> int32;
    auto to_index(int32 x, int32 y, int32 z) const -> int32;
    auto to_index(FIntVector3 coord) const -> int32;
    auto to_index(FVector3f pos) const -> int32;
    void rebuild_static_grid();

    FTestEntityRegistry const* entity_registry_{nullptr};

    FIntVector3 grid_dims_{FIntVector3::ZeroValue};
    FVector3f cell_dims_{FVector3f::ZeroVector};

    TArray<int32> cell_entity_offsets_;
    TArray<uint16> cell_entity_counts_;
    TArray<int32> cell_entity_write_indexes_;
    TArray<int32> non_empty_cell_indices_;
    TArray<FRegistryEntityHandle> entities_;
    WorldAABBs aabbs_;

    FEntityCellData entities_buffer_;

    WorldAABBs static_aabbs_;
    TArray<int32> cell_static_range_indices_;
    TArray<uint32> static_cell_range_offsets_;
    TArray<uint16> static_cell_range_counts_;
    TArray<int32> static_aabb_indices_;
};
}
