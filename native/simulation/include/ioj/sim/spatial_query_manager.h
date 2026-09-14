#pragma once

#include <array>
#include <cstdint>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/line_traces.h>
#include <span>

#include <ioj/sim/query_thread_buffer_pool.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/sim/spatial_query_telemetry.h>

#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/trace_hits.h>

#include <utility>

struct EntityRegistry;

namespace ioj::sim {
struct SpatialQueryManager;
}

namespace ioj::sim::query_manager {
using ThreadBuffers = ioj::sim::QueryThreadBuffers;

class ThreadBufferLease {
  public:
    explicit ThreadBufferLease(SpatialQueryManager const& manager);
    ~ThreadBufferLease();

    ThreadBufferLease(ThreadBufferLease const&) = delete;
    ThreadBufferLease(ThreadBufferLease&&) = delete;
    auto operator=(ThreadBufferLease const&) -> ThreadBufferLease& = delete;
    auto operator=(ThreadBufferLease&&) -> ThreadBufferLease& = delete;

    auto get() const -> ThreadBuffers&;
  private:
    SpatialQueryManager const& manager;
    std::int32_t index;
};
}

namespace ioj::sim {
using SpatialQueryTelemetrySnapshot = ioj::sim::collision::SpatialQueryTelemetrySnapshot;
struct SpatialQueryManager {
  public:
    /* **************************************** */
    // Construction and setup
    /* **************************************** */
    explicit SpatialQueryManager(EntityRegistry const& entity_registry);
    SpatialQueryManager(SpatialQueryManager const&) = delete;
    SpatialQueryManager(SpatialQueryManager&&) = delete;
    auto operator=(SpatialQueryManager const&) -> SpatialQueryManager& = delete;
    auto operator=(SpatialQueryManager&&) -> SpatialQueryManager& = delete;

    void initialise(ioj::sim::collision::CellCoord const grid_dimensions,
                    ioj::sim::Vector3f const cell_size,
                    ioj::sim::collision::EntityAABBs const& entity_bounds);

    void reserve_thread_buffers(std::int32_t count);

    /* **************************************** */
    // Batched line queries
    /* **************************************** */
    void trace_line_of_sight(ioj::sim::Vectors3fConstView start_locations,
                             ioj::sim::Vectors3fConstView end_locations,
                             std::span<RegistryEntityHandle> out_entity_handles) const;
    void has_line_of_sight_to_targets(ioj::sim::Vector3f const& start_location,
                                      ioj::sim::Vectors3fConstView end_locations,
                                      std::span<RegistryEntityHandle const> targets,
                                      std::span<std::uint8_t> has_los) const;
    void have_clear_lines(ioj::sim::Vectors3fConstView start_locations,
                          ioj::sim::Vectors3fConstView end_locations,
                          std::span<std::uint8_t> clear_lines,
                          std::span<RegistryEntityHandle const> ignored_entities = {}) const;
    void trace_closest_lines(ioj::sim::Vectors3fConstView start_locations,
                             ioj::sim::Vectors3fConstView end_locations,
                             TraceHitsView out_hits,
                             std::span<RegistryEntityHandle const> ignored_entities = {}) const;
    void sweep_closest_aabbs(ioj::sim::Vectors3fConstView start_locations,
                             ioj::sim::Vectors3fConstView end_locations,
                             ioj::sim::Vector3f moving_half_extent,
                             TraceHitsView out_hits,
                             std::span<RegistryEntityHandle const> ignored_entities = {},
                             ioj::sim::collision::TraceEntityFilter entity_filter =
                                 ioj::sim::collision::TraceEntityFilter::None) const;

    /* **************************************** */
    // Scalar and entity queries
    /* **************************************** */
    auto has_clear_line(ioj::sim::Vector3f start_location,
                        ioj::sim::Vector3f end_location,
                        RegistryEntityHandle ignored_entity = {}) const -> bool;
    auto trace_closest(ioj::sim::Vector3f start_location,
                       ioj::sim::Vector3f end_location,
                       RegistryEntityHandle ignored_entity = {}) const -> LineTraceResult;

    auto
        collect_non_team_entities_in_range(ioj::sim::Vector3f const& origin,
                                           ioj::sim::Team const team,
                                           float const radius,
                                           std::span<RegistryEntityHandle> const out_entities) const
        -> std::int32_t;
    auto collect_entities_of_type_in_range(ioj::sim::Vector3f const& origin,
                                           ioj::sim::EntityType entity_type,
                                           float radius,
                                           RegistryEntityHandle ignored_entity,
                                           std::span<RegistryEntityHandle> out_entities) const
        -> std::int32_t;
    auto get_any_non_team_entity(ioj::sim::Team const team) const -> RegistryEntityHandle;
    auto get_any_non_team_entity(ioj::sim::Team const team,
                                 ioj::sim::EntityType const entity_type) const
        -> RegistryEntityHandle;
    void are_spheres_in_bounds(ioj::sim::Vectors3fConstView centres,
                               float radius,
                               std::span<std::uint8_t> out_results) const;
    auto get_entity_type_radius(ioj::sim::EntityType entity_type) const noexcept -> float;
    auto get_entity_type_radii() const noexcept -> std::span<float const>;
    void copy_entity_radii(std::span<RegistryEntityHandle const> handles,
                           std::span<float> out_radii) const;

    /* **************************************** */
    // Collision state and telemetry
    /* **************************************** */
    auto get_collision_system() noexcept -> ioj::sim::collision::CollisionSystem& {
        return collision;
    }
    auto get_collision_system() const noexcept -> ioj::sim::collision::CollisionSystem const& {
        return collision;
    }

    auto update(ioj::sim::SimTick tick) -> ioj::sim::collision::DetectedOverlapsView;
    void reset_runtime_telemetry() noexcept;
    auto get_runtime_telemetry() const noexcept -> SpatialQueryTelemetrySnapshot;
  private:
    /* **************************************** */
    // Thread buffer leasing
    /* **************************************** */
    friend class query_manager::ThreadBufferLease;

    using ThreadBuffers = query_manager::ThreadBuffers;

    auto acquire_thread_buffer() const -> std::int32_t;
    void release_thread_buffer(std::int32_t index) const;

    /* **************************************** */
    // State
    /* **************************************** */
    EntityRegistry const& entity_registry;

    mutable ioj::sim::QueryThreadBufferPool thread_buffer_pool_;

    ioj::sim::collision::CollisionSystem collision;
    std::array<float, static_cast<std::size_t>(ioj::sim::EntityType::COUNT)> entity_radii_{};
    ioj::sim::collision::SpatialQueryTelemetry telemetry_;
};
}
