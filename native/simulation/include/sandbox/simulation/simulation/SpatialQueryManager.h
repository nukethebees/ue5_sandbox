#pragma once

#include <array>
#include <cstdint>
#include <sandbox/simulation/entity_types.h>
#include <sandbox/simulation/line_traces.h>
#include <span>

#include <sandbox/simulation/query_thread_buffer_pool.h>
#include <sandbox/simulation/sim_tick.h>
#include <sandbox/simulation/spatial_query_telemetry.h>

#include <sandbox/simulation/simulation/CollisionSystem.h>
#include <sandbox/simulation/simulation/TraceHits.h>

#include <utility>

struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::query_manager {
using FThreadBuffers = simulation::QueryThreadBuffers;

class FThreadBufferLease {
  public:
    explicit FThreadBufferLease(FSpatialQueryManager const& manager);
    ~FThreadBufferLease();

    FThreadBufferLease(FThreadBufferLease const&) = delete;
    FThreadBufferLease(FThreadBufferLease&&) = delete;
    auto operator=(FThreadBufferLease const&) -> FThreadBufferLease& = delete;
    auto operator=(FThreadBufferLease&&) -> FThreadBufferLease& = delete;

    auto get() const -> FThreadBuffers&;
  private:
    FSpatialQueryManager const& manager;
    std::int32_t index;
};
}

namespace ml {
using FSpatialQueryTelemetrySnapshot = simulation::collision::SpatialQueryTelemetrySnapshot;
using FLineTraceResult = simulation::LineTraceResult;

struct FSpatialQueryManager {
  public:
    /* **************************************** */
    // Construction and setup
    /* **************************************** */
    explicit FSpatialQueryManager(FTestEntityRegistry const& entity_registry);
    FSpatialQueryManager(FSpatialQueryManager const&) = delete;
    FSpatialQueryManager(FSpatialQueryManager&&) = delete;
    auto operator=(FSpatialQueryManager const&) -> FSpatialQueryManager& = delete;
    auto operator=(FSpatialQueryManager&&) -> FSpatialQueryManager& = delete;

    void initialise(ml::simulation::collision::CellCoord const grid_dimensions,
                    ml::simulation::Vector3f const cell_size,
                    simulation::collision::EntityAABBs const& entity_bounds);

    void reserve_thread_buffers(std::int32_t count);

    /* **************************************** */
    // Batched line queries
    /* **************************************** */
    void trace_line_of_sight(ml::simulation::Vectors3fConstView start_locations,
                             ml::simulation::Vectors3fConstView end_locations,
                             std::span<FRegistryEntityHandle> out_entity_handles) const;
    void has_line_of_sight_to_targets(ml::simulation::Vector3f const& start_location,
                                      ml::simulation::Vectors3fConstView end_locations,
                                      std::span<FRegistryEntityHandle const> targets,
                                      std::span<std::uint8_t> has_los) const;
    void have_clear_lines(ml::simulation::Vectors3fConstView start_locations,
                          ml::simulation::Vectors3fConstView end_locations,
                          std::span<std::uint8_t> clear_lines,
                          std::span<FRegistryEntityHandle const> ignored_entities = {}) const;
    void trace_closest_lines(ml::simulation::Vectors3fConstView start_locations,
                             ml::simulation::Vectors3fConstView end_locations,
                             FTraceHitsView out_hits,
                             std::span<FRegistryEntityHandle const> ignored_entities = {}) const;
    void sweep_closest_aabbs(
        ml::simulation::Vectors3fConstView start_locations,
        ml::simulation::Vectors3fConstView end_locations,
        ml::simulation::Vector3f moving_half_extent,
        FTraceHitsView out_hits,
        std::span<FRegistryEntityHandle const> ignored_entities = {},
        ioj::ETraceEntityFilter entity_filter = ioj::ETraceEntityFilter::None) const;

    /* **************************************** */
    // Scalar and entity queries
    /* **************************************** */
    auto has_clear_line(ml::simulation::Vector3f start_location,
                        ml::simulation::Vector3f end_location,
                        FRegistryEntityHandle ignored_entity = {}) const -> bool;
    auto trace_closest(ml::simulation::Vector3f start_location,
                       ml::simulation::Vector3f end_location,
                       FRegistryEntityHandle ignored_entity = {}) const -> FLineTraceResult;

    auto collect_non_team_entities_in_range(
        ml::simulation::Vector3f const& origin,
        simulation::Team const team,
        float const radius,
        std::span<FRegistryEntityHandle> const out_entities) const -> std::int32_t;
    auto collect_entities_of_type_in_range(ml::simulation::Vector3f const& origin,
                                           simulation::EntityType entity_type,
                                           float radius,
                                           FRegistryEntityHandle ignored_entity,
                                           std::span<FRegistryEntityHandle> out_entities) const
        -> std::int32_t;
    auto get_any_non_team_entity(simulation::Team const team) const -> FRegistryEntityHandle;
    auto get_any_non_team_entity(simulation::Team const team,
                                 simulation::EntityType const entity_type) const
        -> FRegistryEntityHandle;
    void are_spheres_in_bounds(ml::simulation::Vectors3fConstView centres,
                               float radius,
                               std::span<std::uint8_t> out_results) const;
    auto get_entity_type_radius(simulation::EntityType entity_type) const noexcept -> float;
    auto get_entity_type_radii() const noexcept -> std::span<float const>;
    void copy_entity_radii(std::span<FRegistryEntityHandle const> handles,
                           std::span<float> out_radii) const;

    /* **************************************** */
    // Collision state and telemetry
    /* **************************************** */
    auto get_collision_system() noexcept -> ioj::FCollisionSystem& { return collision; }
    auto get_collision_system() const noexcept -> ioj::FCollisionSystem const& { return collision; }

    auto update(simulation::SimTick tick) -> ioj::FDetectedOverlapsView;
    void reset_runtime_telemetry() noexcept;
    auto get_runtime_telemetry() const noexcept -> FSpatialQueryTelemetrySnapshot;
  private:
    /* **************************************** */
    // Thread buffer leasing
    /* **************************************** */
    friend class query_manager::FThreadBufferLease;

    using FThreadBuffers = query_manager::FThreadBuffers;

    auto acquire_thread_buffer() const -> std::int32_t;
    void release_thread_buffer(std::int32_t index) const;

    /* **************************************** */
    // State
    /* **************************************** */
    FTestEntityRegistry const& entity_registry;

    mutable simulation::QueryThreadBufferPool thread_buffer_pool_;

    ioj::FCollisionSystem collision;
    std::array<float, static_cast<std::size_t>(simulation::EntityType::COUNT)> entity_radii_{};
    simulation::collision::SpatialQueryTelemetry telemetry_;
};
}
