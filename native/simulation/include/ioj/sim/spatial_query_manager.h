#pragma once

#include <array>
#include <cstdint>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/line_traces.h>
#include <span>

#include <ioj/sim/query_thread_buffer_pool.h>
#include <ioj/sim/sim_tick.h>

#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/trace_hits.h>

#include <utility>

struct EntityRegistry;

namespace ioj::sim {
struct SpatialQueryManager;
}

namespace ioj::sim::query_manager {
using ThreadBuffers = QueryThreadBuffers;

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
struct SpatialQueryManager {
  public:
    /* **************************************** */
    // Construction and setup
    /* **************************************** */
    SpatialQueryManager(EntityRegistry const& entity_registry, AgentAccessor const& agents);
    SpatialQueryManager(SpatialQueryManager const&) = delete;
    SpatialQueryManager(SpatialQueryManager&&) = delete;
    auto operator=(SpatialQueryManager const&) -> SpatialQueryManager& = delete;
    auto operator=(SpatialQueryManager&&) -> SpatialQueryManager& = delete;

    void initialise(collision::CellCoord const grid_dimensions,
                    Vector3f const cell_size,
                    collision::EntityAABBs const& entity_bounds);

    void reserve_thread_buffers(std::int32_t count);

    /* **************************************** */
    // Batched line queries
    /* **************************************** */
    void trace_line_of_sight(Vectors3fConstView start_locations,
                             Vectors3fConstView end_locations,
                             std::span<EntityUniqueId> out_entity_ids) const;
    void has_line_of_sight_to_targets(Vector3f const& start_location,
                                      Vectors3fConstView end_locations,
                                      std::span<EntityUniqueId const> targets,
                                      std::span<std::uint8_t> has_los) const;
    void have_clear_lines(Vectors3fConstView start_locations,
                          Vectors3fConstView end_locations,
                          std::span<std::uint8_t> clear_lines,
                          std::span<EntityUniqueId const> ignored_entities = {}) const;
    void trace_closest_lines(Vectors3fConstView start_locations,
                             Vectors3fConstView end_locations,
                             TraceHitsView out_hits,
                             std::span<EntityUniqueId const> ignored_entities = {}) const;
    void sweep_closest_aabbs(
        Vectors3fConstView start_locations,
        Vectors3fConstView end_locations,
        Vector3f moving_half_extent,
        TraceHitsView out_hits,
        std::span<EntityUniqueId const> ignored_entities = {},
        collision::TraceEntityFilter entity_filter = collision::TraceEntityFilter::None) const;

    /* **************************************** */
    // Scalar and entity queries
    /* **************************************** */
    auto has_clear_line(Vector3f start_location,
                        Vector3f end_location,
                        EntityUniqueId ignored_entity = {}) const -> bool;
    auto trace_closest(Vector3f start_location,
                       Vector3f end_location,
                       EntityUniqueId ignored_entity = {}) const -> LineTraceResult;

    auto collect_non_team_entities_in_range(Vector3f const& origin,
                                            Team const team,
                                            float const radius,
                                            std::span<EntityUniqueId> const out_entities) const
        -> std::int32_t;
    auto collect_entities_of_type_in_range(Vector3f const& origin,
                                           EntityType entity_type,
                                           float radius,
                                           EntityUniqueId ignored_entity,
                                           std::span<EntityUniqueId> out_entities) const
        -> std::int32_t;
    auto get_any_non_team_entity(Team const team) const -> EntityUniqueId;
    auto get_any_non_team_entity(Team const team, EntityType const entity_type) const
        -> EntityUniqueId;
    void are_spheres_in_bounds(Vectors3fConstView centres,
                               float radius,
                               std::span<std::uint8_t> out_results) const;
    auto get_entity_type_radius(EntityType entity_type) const noexcept -> float;
    auto get_entity_type_radii() const noexcept -> std::span<float const>;
    void copy_entity_radii(std::span<EntityUniqueId const> ids, std::span<float> out_radii) const;

    /* **************************************** */
    // Collision state and telemetry
    /* **************************************** */
    auto get_collision_system() noexcept -> collision::CollisionSystem& { return collision; }
    auto get_collision_system() const noexcept -> collision::CollisionSystem const& {
        return collision;
    }

    auto update(std::span<EntityUniqueId const> dirty_entities, SimTick tick)
        -> collision::DetectedOverlapsView;
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
    AgentAccessor const& agents_;

    mutable QueryThreadBufferPool thread_buffer_pool_;

    collision::CollisionSystem collision;
    std::array<float, static_cast<std::size_t>(EntityType::COUNT)> entity_radii_{};
};
}
