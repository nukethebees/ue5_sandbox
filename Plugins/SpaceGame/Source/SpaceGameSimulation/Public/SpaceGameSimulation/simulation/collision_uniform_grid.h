#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SGCollision/world_aabbs.h>
#include <SpaceGameSimulation/simulation/EntityAABBs.h>
#include <SpaceGameSimulation/simulation/EntityCellData.h>

#include <CoreMinimal.h>
#include <SandboxCore/soa_vectors.h>

#include <atomic>

class UStaticMesh;
struct FTestEntityRegistry;

namespace ml {
struct FLineTracesConstView;
struct FTraceHitsView;
}

namespace ml::ioj {
enum class ETraceEntityFilter : uint8 {
    None,
    ExcludeCapitalShipFighters,
};

struct FCellCoordBounds {
    FIntVector3 min;
    FIntVector3 max;
};

struct FCollisionGridTelemetrySnapshot {
    uint64 rebuild_count{};
    uint64 line_trace_count{};
    uint64 sweep_trace_count{};
};

struct SPACEGAMESIMULATION_API CollisionUniformGrid {
    static inline FVector3f const origin{FVector3f::ZeroVector};

    explicit CollisionUniformGrid(FTestEntityRegistry const& entity_registry) noexcept;
    CollisionUniformGrid(CollisionUniformGrid const&) = delete;
    CollisionUniformGrid(CollisionUniformGrid&&) = delete;
    auto operator=(CollisionUniformGrid const&) -> CollisionUniformGrid& = delete;
    auto operator=(CollisionUniformGrid&&) -> CollisionUniformGrid& = delete;

    auto is_configured() const noexcept -> bool;

    auto get_grid_dims() const noexcept -> FIntVector3;
    void set_grid_dims(FIntVector3 const grid_dims) noexcept;

    auto get_cell_dims() const noexcept -> FVector3f;
    void set_cell_dims(FVector3f const cell_dims) noexcept;

    auto num_cells() const -> int32;
    auto get_non_empty_cell_count() const noexcept -> int32 {
        return non_empty_cell_indices_.Num();
    }
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
    void are_spheres_in_bounds(FVectors3f::ConstView centres,
                               float radius,
                               TArrayView<uint8> out_results) const;
    static auto to_string(FIntVector3 value) -> FString;

    void reset();
    void set_static_aabbs(WorldAABBs static_aabbs);
    auto add_static_aabb(FVector3f min_point, FVector3f max_point) -> int32;
    void rebuild_grid(FEntityAABBs const& entity_aabbs);
    void reset_runtime_telemetry() noexcept;
    auto get_runtime_telemetry() const noexcept -> FCollisionGridTelemetrySnapshot;

    auto get_static_aabbs() const noexcept -> WorldAABBs const& { return static_aabbs_; }
    auto get_entity_world_bounds() const -> WorldAABBs::ConstView {
        return {entities_buffer_.min_points.get_const_view(),
                entities_buffer_.max_points.get_const_view()};
    }

    // Appends exact overlaps. Multi-cell participants may be appended more than once.
    void append_overlaps(FBox3f const& query_bounds,
                         FRegistryEntityHandle ignored_entity,
                         TArray<FRegistryEntityHandle>& out_entities,
                         TArray<int32>& out_static_geometry_indices) const;
    void trace_aabbs(FLineTracesConstView const& traces, FTraceHitsView const& hits) const;
    void trace_aabbs(FLineTracesConstView const& traces,
                     FTraceHitsView const& hits,
                     TConstArrayView<FRegistryEntityHandle> ignored_entities) const;
    void sweep_aabbs(FLineTracesConstView const& centre_paths,
                     FVector3f moving_half_extent,
                     FTraceHitsView const& hits,
                     TConstArrayView<FRegistryEntityHandle> ignored_entities = {},
                     ETraceEntityFilter entity_filter = ETraceEntityFilter::None) const;
  private:
    enum class ETraceKind : uint8 {
        Line,
        Sweep,
    };

    enum class EIgnoredEntityMode : uint8 {
        None,
        PerTrace,
    };

    template <ETraceKind TraceKind>
    static auto trace_aabb(WorldAABBs::ConstView const& aabbs,
                           int32 aabb_index,
                           FVector3f trace_start,
                           FVector3f inverse_trace_delta,
                           FVector3f trace_delta,
                           FVector3f expansion) -> float;

    template <ETraceKind TraceKind,
              EIgnoredEntityMode IgnoredEntityMode,
              ETraceEntityFilter EntityFilter>
    void trace_aabbs_impl(FLineTracesConstView const& traces,
                          FTraceHitsView const& hits,
                          TConstArrayView<FRegistryEntityHandle> ignored_entities,
                          FVector3f moving_half_extent) const;

    auto to_cell_x(float value) const -> int32;
    auto to_cell_y(float value) const -> int32;
    auto to_cell_z(float value) const -> int32;
    auto to_index(int32 x, int32 y, int32 z) const -> int32;
    auto to_index(FIntVector3 coord) const -> int32;
    auto to_index(FVector3f pos) const -> int32;
    void rebuild_static_grid();

    FTestEntityRegistry const& entity_registry_;

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

    std::atomic<uint64> rebuild_count_{};
    mutable std::atomic<uint64> line_trace_count_{};
    mutable std::atomic<uint64> sweep_trace_count_{};
};
}
