#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/simulation/CollisionSystem.h>
#include <SpaceGame/simulation/LineTraces.h>
#include <SpaceGame/simulation/TraceHits.h>

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <CoreMinimal.h>
#include <SandboxCore/soa_vectors.h>

#include <atomic>
#include <mutex>
#include <utility>

struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::query_manager {
struct FThreadBuffers {
    FLineTraces line_traces;
    FTraceHits trace_hits;
    TArray<uint32> range_query_entity_stamps;
    uint32 range_query_stamp{};
};

class FThreadBufferLease {
  public:
    explicit FThreadBufferLease(FSpatialQueryManager const& manager);
    ~FThreadBufferLease();

    FThreadBufferLease(FThreadBufferLease const&) = delete;
    auto operator=(FThreadBufferLease const&) -> FThreadBufferLease& = delete;

    auto get() const -> FThreadBuffers&;
  private:
    FSpatialQueryManager const* manager;
    int32 index;
};
}

namespace ml {
struct FSpatialQueryTelemetrySnapshot {
    uint64 grid_rebuild_count{};
    uint64 range_query_count{};
    uint64 line_trace_count{};
    uint64 sweep_trace_count{};
    int32 occupied_dynamic_cell_count{};
};

struct FLineTraceResult {
    FVector3f location{FVector3f::ZeroVector};
    FRegistryEntityHandle entity;
    int32 static_geometry_index{INDEX_NONE};
    bool hit{false};
};

struct SPACEGAME_API FSpatialQueryManager {
  public:
    FSpatialQueryManager() = default;

    void initialise(FTestEntityRegistry const& entity_registry,
                    FIntVector3 const grid_dimensions,
                    FVector3f const cell_size,
                    ioj::FEntityAABBs const& entity_bounds);

    void reserve_thread_buffers(int32 count);

    void trace_line_of_sight(FVectors3f::ConstView start_locations,
                             FVectors3f::ConstView end_locations,
                             TArrayView<FRegistryEntityHandle> out_entity_handles) const;
    void has_line_of_sight_to_targets(FVector3f const& start_location,
                                      FVectors3f::ConstView end_locations,
                                      TConstArrayView<FRegistryEntityHandle> targets,
                                      TArrayView<uint8> has_los) const;
    void have_clear_lines(FVectors3f::ConstView start_locations,
                          FVectors3f::ConstView end_locations,
                          TArrayView<uint8> clear_lines,
                          TConstArrayView<FRegistryEntityHandle> ignored_entities = {}) const;
    void trace_closest_lines(FVectors3f::ConstView start_locations,
                             FVectors3f::ConstView end_locations,
                             FTraceHitsView out_hits,
                             TConstArrayView<FRegistryEntityHandle> ignored_entities = {}) const;
    void sweep_closest_aabbs(
        FVectors3f::ConstView start_locations,
        FVectors3f::ConstView end_locations,
        FVector3f moving_half_extent,
        FTraceHitsView out_hits,
        TConstArrayView<FRegistryEntityHandle> ignored_entities = {},
        ioj::ETraceEntityFilter entity_filter = ioj::ETraceEntityFilter::None) const;
    auto has_clear_line(FVector3f start_location,
                        FVector3f end_location,
                        FRegistryEntityHandle ignored_entity = {}) const -> bool;
    auto trace_closest(FVector3f start_location,
                       FVector3f end_location,
                       FRegistryEntityHandle ignored_entity = {}) const -> FLineTraceResult;

    auto collect_non_team_entities_in_range(
        FVector3f const& origin,
        ETestTeam const team,
        float const radius,
        TArrayView<FRegistryEntityHandle> const out_entities) const -> int32;
    auto collect_entities_of_type_in_range(FVector3f const& origin,
                                           ETestEntityType entity_type,
                                           float radius,
                                           FRegistryEntityHandle ignored_entity,
                                           TArrayView<FRegistryEntityHandle> out_entities) const
        -> int32;
    auto get_any_non_team_entity(ETestTeam const team) const -> FRegistryEntityHandle;
    auto get_any_non_team_entity(ETestTeam const team, ETestEntityType const entity_type) const
        -> FRegistryEntityHandle;
    void are_spheres_in_bounds(FVectors3f::ConstView centres,
                               float radius,
                               TArrayView<uint8> out_results) const;

    auto get_collision_system() noexcept -> ioj::FCollisionSystem& { return collision; }
    auto get_collision_system() const noexcept -> ioj::FCollisionSystem const& { return collision; }

    void update();
    void reset_runtime_telemetry() noexcept;
    auto get_runtime_telemetry() const noexcept -> FSpatialQueryTelemetrySnapshot;
  private:
    friend class query_manager::FThreadBufferLease;

    using FThreadBuffers = query_manager::FThreadBuffers;

    auto acquire_thread_buffer() const -> int32;
    void release_thread_buffer(int32 index) const;

    FTestEntityRegistry const* entity_registry{nullptr};

    mutable std::mutex thread_buffers_mutex;
    mutable TArray<FThreadBuffers> thread_buffers;
    mutable TArray<int32> free_thread_buffer_indices;
    mutable int32 active_thread_buffer_count{};

    ioj::FCollisionSystem collision;
    mutable std::atomic<uint64> range_query_count_{};
};
}
