#include "ioj/sim/spatial_query_manager.h"

#include <ioj/sim/column_math.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/line_trace_batch.h>
#include <ioj/sim/profiling.h>

#include <sandbox/core/diagnostics.h>
#include <sandbox/core/frame_memory_resource.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <format>
#include <utility>

namespace {
enum class QueryMode : std::uint8_t {
    HitEntity,
    ClearLine,
    TargetLineOfSight,
    ClosestHit,
};

struct TraceRequest {
    ioj::sim::Vectors3fConstView start_locations{};
    ioj::sim::Vectors3fConstView end_locations{};
    ioj::sim::Vector3f scalar_start{};
    ioj::sim::Vector3f scalar_end{};
    std::span<ioj::sim::EntityUniqueId const> targets{};
    std::span<ioj::sim::EntityUniqueId const> ignored_entities{};
    std::span<ioj::sim::EntityUniqueId> out_entity_ids{};
    std::span<ioj::sim::LineQueryResult> out_flags{};
};

template <QueryMode Mode>
auto trace_impl(ioj::sim::collision::CollisionUniformGrid const& uniform_grid,
                ml::FrameMemoryResource* const scratch_resource,
                TraceRequest const& request) -> ioj::sim::LineTraceResult {
    auto const count{[&] {
        if constexpr (Mode == QueryMode::ClosestHit) {
            return 1;
        } else {
            return request.end_locations.num();
        }
    }()};

    if constexpr (Mode == QueryMode::HitEntity) {
        assert(count == request.start_locations.num());
        assert(static_cast<std::size_t>(count) == request.out_entity_ids.size());
        std::ranges::fill(request.out_entity_ids, ioj::sim::EntityUniqueId{});
    } else if constexpr (Mode == QueryMode::ClearLine) {
        assert(count == request.start_locations.num());
        assert(static_cast<std::size_t>(count) == request.out_flags.size());
        assert(request.ignored_entities.empty() ||
               request.ignored_entities.size() == static_cast<std::size_t>(count));
        std::ranges::fill(request.out_flags, ioj::sim::LineQueryResult{});
    } else if constexpr (Mode == QueryMode::TargetLineOfSight) {
        assert(static_cast<std::size_t>(count) == request.targets.size());
        assert(static_cast<std::size_t>(count) == request.out_flags.size());
        std::ranges::fill(request.out_flags, ioj::sim::LineQueryResult{});
    } else {
        assert(request.ignored_entities.size() == 1);
    }

    if (count == 0) {
        return {};
    }

    ioj::sim::LineTraces traces{scratch_resource};
    ioj::sim::TraceHits trace_hits{scratch_resource};
    trace_hits.set_num(count);
    auto const hits{trace_hits.get_view()};
    auto const hit_flags{hits.hits()};
    auto const hit_entities{hits.entities()};

    auto const trace_view{[&] {
        if constexpr (Mode == QueryMode::TargetLineOfSight || Mode == QueryMode::ClosestHit) {
            traces.set_num(count);
            auto const trace_columns{traces.get_view()};
            auto const starts{trace_columns.view_starts()};
            auto const ends{trace_columns.view_ends()};
            for (std::uint32_t i{}; i < count; ++i) {
                ioj::sim::set_vector(starts, i, request.scalar_start);
                auto const end{Mode == QueryMode::TargetLineOfSight ? request.end_locations[i]
                                                                    : request.scalar_end};
                ioj::sim::set_vector(ends, i, end);
            }
            return ioj::sim::LineTraceBatch{traces.get_const_view()};
        } else {
            return ioj::sim::LineTraceBatch{request.start_locations, request.end_locations};
        }
    }()};

    if constexpr (Mode == QueryMode::ClosestHit) {
        uniform_grid.trace_aabbs(trace_view, hits, request.ignored_entities);
    } else if constexpr (Mode == QueryMode::ClearLine) {
        if (request.ignored_entities.empty()) {
            uniform_grid.trace_aabbs(trace_view, hits);
        } else {
            uniform_grid.trace_aabbs(trace_view, hits, request.ignored_entities);
        }
    } else {
        uniform_grid.trace_aabbs(trace_view, hits);
    }

    if constexpr (Mode == QueryMode::ClosestHit) {
        return {
            .location = hits.view_locations()[0],
            .entity = hit_entities[0],
            .static_geometry_index = hits.static_geometry_indices()[0],
            .hit = hit_flags[0] != 0,
        };
    } else {
        if constexpr (Mode == QueryMode::HitEntity) {
            for (std::uint32_t i{}; i < count; ++i) {
                request.out_entity_ids[i] = hit_entities[i];
            }
        } else if constexpr (Mode == QueryMode::ClearLine) {
            for (std::uint32_t i{}; i < count; ++i) {
                request.out_flags[i] = static_cast<ioj::sim::LineQueryResult>(hit_flags[i] == 0);
            }
        } else if constexpr (Mode == QueryMode::TargetLineOfSight) {
            for (std::uint32_t i{}; i < count; ++i) {
                request.out_flags[i] = static_cast<ioj::sim::LineQueryResult>(
                    hit_flags[i] == 0 || hit_entities[i] == request.targets[i]);
            }
        }

        return {};
    }
}

void validate_grid_for_range_query(ioj::sim::collision::CollisionUniformGrid const& grid,
                                   ioj::sim::Vector3f const& origin,
                                   float const radius) {
    if (!grid.is_configured()) {
        ml::fatal_error(std::format(
            "Cannot query unconfigured grid: origin=({}, {}, {}), radius={}, dimensions={}",
            origin.X,
            origin.Y,
            origin.Z,
            radius,
            ioj::sim::collision::CollisionUniformGrid::to_string(grid.get_grid_dims())));
    }
}
}

namespace ioj::sim {
namespace {
template <typename IncludeEntity>
auto collect_entities_in_range(collision::CollisionUniformGrid const& grid,
                               EntityTables const& agents,
                               ml::EnumArray<EntityType, Vectors3fConstView> const& locations,
                               ml::FrameMemoryResource* const scratch_resource,
                               Vector3f const origin,
                               float const radius,
                               std::span<EntityUniqueId> const out_entities,
                               IncludeEntity&& include_entity) -> std::uint32_t {
    if (out_entities.empty()) {
        return 0;
    }

    auto const absolute_radius{std::abs(radius)};
    auto const radius_extent{ml::make_vector3f(absolute_radius, absolute_radius, absolute_radius)};
    auto [min_coord,
          max_coord]{grid.to_cell_coord_bounds(origin - radius_extent, origin + radius_extent)};
    auto const max_grid_coord{grid.get_max_grid_coord()};
    if (max_coord.x < 0 || max_coord.y < 0 || max_coord.z < 0 || min_coord.x > max_grid_coord.x ||
        min_coord.y > max_grid_coord.y || min_coord.z > max_grid_coord.z) {
        return 0;
    }

    min_coord = min_coord.component_max({});
    max_coord = max_coord.component_min(max_grid_coord);

    ml::FrameArray<std::uint8_t> visited{scratch_resource};
    EntityTypeSizes offsets;
    std::uint32_t total_count{};
    auto const entity_type_count{EntityTypeSizes::size()};
    for (std::size_t i{}; i < entity_type_count; ++i) {
        auto const type{static_cast<EntityType>(i)};
        offsets[type] = total_count;
        total_count += agents.lookups.for_type(type).row_count();
    }
    visited.set_num(total_count);
    auto const radius_squared{radius * radius};
    std::array<std::span<EntityInstanceHandle const>, EntityTypeSizes::size()> handles;
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        // Bind each type's table once before scanning.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        handles[std::to_underlying(type)] = agents.lookups.for_type(type).entries();
    }
    std::uint32_t count{};

    for (auto x{min_coord.x}; x <= max_coord.x; ++x) {
        for (auto y{min_coord.y}; y <= max_coord.y; ++y) {
            for (auto z{min_coord.z}; z <= max_coord.z; ++z) {
                // Each grid coordinate selects a different cell.
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
                for (auto const id : grid.get_cell_entities({x, y, z})) {
                    auto const handle{handles[std::to_underlying(id.entity_type())][id.index()]};
                    if (!include_entity(id, handle.team())) {
                        continue;
                    }
                    auto const local_index{handle.index()};

                    auto const entity_index{offsets[id.entity_type()] + local_index};
                    if (visited[entity_index]) {
                        continue;
                    }
                    visited[entity_index] = 1;

                    if (HMM_LenSqrV3(locations[id.entity_type()][local_index] - origin) <=
                        radius_squared) {
                        out_entities[count++] = id;
                        if (count == out_entities.size()) {
                            return count;
                        }
                    }
                }
            }
        }
    }

    return count;
}
} // namespace

/* **************************************** */
// Construction and setup
/* **************************************** */
SpatialQueryManager::SpatialQueryManager(EntityTables const& agents,
                                         std::pmr::memory_resource* resource)
    : entity_tables_{agents}
    , collision_system_{agents, resource} {}

void SpatialQueryManager::initialise(collision::GridGeometry const grid_geometry,
                                     collision::EntityAABBs const& entity_bounds) {
    collision_system_.initialise(grid_geometry, entity_bounds);

    for (auto const type : ml::EnumTraits<EntityType>::values) {
        entity_radii_[type] = collision::get_entity_radius(entity_bounds, type);
    }
}

/* **************************************** */
// Batched line queries
/* **************************************** */
void SpatialQueryManager::trace_line_of_sight(
    Vectors3fConstView const start_locations,
    Vectors3fConstView const end_locations,
    std::span<EntityUniqueId> const out_entity_ids,
    ml::FrameMemoryResource* const scratch_resource) const {
    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::trace_line_of_sight");

    trace_impl<QueryMode::HitEntity>(collision_system_.uniform_grid_,
                                     scratch_resource,
                                     {.start_locations = start_locations,
                                      .end_locations = end_locations,
                                      .out_entity_ids = out_entity_ids});
}

void SpatialQueryManager::has_line_of_sight_to_targets(
    Vector3f const& start_location,
    Vectors3fConstView const end_locations,
    std::span<EntityUniqueId const> const targets,
    std::span<LineQueryResult> const has_los,
    ml::FrameMemoryResource* const scratch_resource) const {
    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::has_line_of_sight_to_targets");

    trace_impl<QueryMode::TargetLineOfSight>(collision_system_.uniform_grid_,
                                             scratch_resource,
                                             {.end_locations = end_locations,
                                              .scalar_start = start_location,
                                              .targets = targets,
                                              .out_flags = has_los});
}

void SpatialQueryManager::have_clear_lines(
    Vectors3fConstView const start_locations,
    Vectors3fConstView const end_locations,
    std::span<LineQueryResult> const clear_lines,
    ml::FrameMemoryResource* const scratch_resource,
    std::span<EntityUniqueId const> const ignored_entities) const {
    trace_impl<QueryMode::ClearLine>(collision_system_.uniform_grid_,
                                     scratch_resource,
                                     {.start_locations = start_locations,
                                      .end_locations = end_locations,
                                      .ignored_entities = ignored_entities,
                                      .out_flags = clear_lines});
}

void SpatialQueryManager::trace_closest_lines(
    Vectors3fConstView const start_locations,
    Vectors3fConstView const end_locations,
    TraceHits::View const out_hits,
    std::span<EntityUniqueId const> const ignored_entities) const {
    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::trace_closest_lines");

    [[maybe_unused]] auto const count{start_locations.num()};
    assert(end_locations.num() == count);
    assert(out_hits.num() == count);
    assert(ignored_entities.empty() || ignored_entities.size() == static_cast<std::size_t>(count));

    auto const traces{LineTraceBatch{start_locations, end_locations}};
    auto const& uniform_grid{collision_system_.uniform_grid_};
    if (ignored_entities.empty()) {
        uniform_grid.trace_aabbs(traces, out_hits);
    } else {
        uniform_grid.trace_aabbs(traces, out_hits, ignored_entities);
    }
}

void SpatialQueryManager::sweep_closest_aabbs(
    Vectors3fConstView const start_locations,
    Vectors3fConstView const end_locations,
    Vector3f const moving_half_extent,
    TraceHits::View const out_hits,
    std::span<EntityUniqueId const> const ignored_entities,
    collision::TraceEntityFilter const entity_filter) const {
    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::sweep_closest_aabbs");

    [[maybe_unused]] auto const count{start_locations.num()};
    assert(end_locations.num() == count);
    assert(out_hits.num() == count);
    assert(ignored_entities.empty() || ignored_entities.size() == static_cast<std::size_t>(count));

    collision_system_.uniform_grid_.sweep_aabbs(LineTraceBatch{start_locations, end_locations},
                                                moving_half_extent,
                                                out_hits,
                                                ignored_entities,
                                                entity_filter);
}

/* **************************************** */
// Scalar and entity queries
/* **************************************** */
void SpatialQueryManager::collect_unique_entities_in_cells(
    std::span<collision::CellCoord const> const cells,
    ml::FrameArray<EntityUniqueId>& out_entities) const {
    collision_system_.uniform_grid_.collect_unique_entities_in_cells(cells, out_entities);
}
auto SpatialQueryManager::has_clear_line(Vector3f const start_location,
                                         Vector3f const end_location,
                                         ml::FrameMemoryResource* const scratch_resource,
                                         EntityUniqueId const ignored_entity) const -> bool {
    return !trace_closest(start_location, end_location, scratch_resource, ignored_entity).hit;
}

auto SpatialQueryManager::trace_closest(Vector3f const start_location,
                                        Vector3f const end_location,
                                        ml::FrameMemoryResource* const scratch_resource,
                                        EntityUniqueId const ignored_entity) const
    -> LineTraceResult {
    std::array<EntityUniqueId, 1> ignored_entities{ignored_entity};
    return trace_impl<QueryMode::ClosestHit>(collision_system_.uniform_grid_,
                                             scratch_resource,
                                             {.scalar_start = start_location,
                                              .scalar_end = end_location,
                                              .ignored_entities = ignored_entities});
}

auto SpatialQueryManager::collect_non_team_entities_in_range(
    Vector3f const& origin,
    Team const team,
    float const radius,
    std::span<EntityUniqueId> const out_entities,
    ml::FrameMemoryResource* const scratch_resource) const -> std::uint32_t {
    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::collect_non_team_entities_in_range");

    if (out_entities.empty()) {
        return 0;
    }

    auto const& grid{collision_system_.uniform_grid_};
    validate_grid_for_range_query(grid, origin, radius);
    {
        SANDBOX_PROFILE_SCOPE(
            "Sandbox::SpatialQueryManager::collect_non_team_entities_in_range::loop");
        return collect_entities_in_range(
            grid,
            entity_tables_,
            locations_,
            scratch_resource,
            origin,
            radius,
            out_entities,
            [team](EntityUniqueId, Team const candidate_team) { return candidate_team != team; });
    }
}

auto SpatialQueryManager::collect_entities_of_type_in_range(
    Vector3f const& origin,
    EntityType const entity_type,
    float const radius,
    EntityUniqueId const ignored_entity,
    std::span<EntityUniqueId> const out_entities,
    ml::FrameMemoryResource* const scratch_resource) const -> std::uint32_t {
    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::collect_entities_of_type_in_range");

    if (out_entities.empty()) {
        return 0;
    }

    auto const& grid{collision_system_.uniform_grid_};
    validate_grid_for_range_query(grid, origin, radius);
    return collect_entities_in_range(grid,
                                     entity_tables_,
                                     locations_,
                                     scratch_resource,
                                     origin,
                                     radius,
                                     out_entities,
                                     [entity_type, ignored_entity](EntityUniqueId const id, Team) {
                                         return id != ignored_entity &&
                                                id.entity_type() == entity_type;
                                     });
}

auto SpatialQueryManager::get_any_non_team_entity(Team const team,
                                                  EntityType const entity_type) const
    -> EntityUniqueId {
    assert(entity_tables_.lookups.permits_lookup());
    auto const handles{entity_tables_.lookups.for_type(entity_type).entries()};
    auto const count{static_cast<std::uint32_t>(handles.size())};
    for (std::uint32_t index{}; index < count; ++index) {
        if (handles[index].is_valid() && handles[index].team() != team) {
            return EntityUniqueId{index, entity_type};
        }
    }
    return {};
}

void SpatialQueryManager::are_spheres_in_bounds(
    Vectors3fConstView const centres,
    float const radius,
    std::span<collision::SphereInBoundsResult> const out_results) const {
    collision_system_.uniform_grid_.are_spheres_in_bounds(centres, radius, out_results);
}

auto SpatialQueryManager::get_entity_type_radius(EntityType const entity_type) const noexcept
    -> float {
    return entity_radii_[entity_type];
}

auto SpatialQueryManager::get_entity_type_radii() const noexcept -> EntityTypeRadii const& {
    return entity_radii_;
}

void SpatialQueryManager::copy_entity_locations(
    std::span<EntityUniqueId const> const ids,
    Vectors3fView const output,
    ml::FrameMemoryResource* const scratch_resource) const {
    assert(ids.size() == output.num());
    ml::FrameArray<std::uint32_t> order{scratch_resource};
    ml::FrameArray<EntityInstanceHandle> handles{scratch_resource};
    order.set_num(output.num());
    handles.set_num(output.num());
    auto const runs{entity_tables_.lookups.lookup_handles(ids, order, handles)};
    for (std::uint32_t row{}; row < output.num(); ++row) {
        output.set(row, {});
    }
    for (std::uint32_t run{}; run < runs.num; ++run) {
        auto const locations{locations_[runs.types[run]]};
        auto const end{runs.end(run)};
        for (auto index{runs.offsets[run]}; index < end; ++index) {
            auto const row{order[index]};
            if (handles[row].is_valid()) {
                output.set(row, locations[handles[row].index()]);
            }
        }
    }
}
void
    SpatialQueryManager::copy_entity_motion(std::span<EntityUniqueId const> const ids,
                                            Vectors3fView const output_locations,
                                            Vectors3fView const output_velocities,
                                            std::span<EntityInstanceHandle> const handles,
                                            ml::FrameMemoryResource* const scratch_resource) const {
    auto const count{output_locations.num()};
    assert(ids.size() == count && output_velocities.num() == count);
    ml::FrameArray<std::uint32_t> order{scratch_resource};
    order.set_num(count);
    auto const runs{entity_tables_.lookups.lookup_handles(ids, order, handles)};
    for (std::uint32_t row{}; row < count; ++row) {
        output_locations.set(row, {});
        output_velocities.set(row, {});
    }
    for (std::uint32_t run{}; run < runs.num; ++run) {
        auto const locations{locations_[runs.types[run]]};
        auto const velocities{velocities_[runs.types[run]]};
        auto const end{runs.end(run)};
        for (auto index{runs.offsets[run]}; index < end; ++index) {
            auto const row{order[index]};
            auto const handle{handles[row]};
            if (handle.is_valid()) {
                output_locations.set(row, locations[handle.index()]);
                if (!velocities.is_empty()) {
                    output_velocities.set(row, velocities[handle.index()]);
                }
            }
        }
    }
}

/* **************************************** */
// Collision and spatial-index lifecycle
/* **************************************** */
void SpatialQueryManager::set_static_collision(collision::WorldAABBs bounds) {
    collision_system_.set_static_collision(std::move(bounds));
}
auto SpatialQueryManager::add_static_collision_aabb(Vector3f const min_point,
                                                    Vector3f const max_point)
    -> collision::StaticGeometryIndex {
    return collision_system_.add_static_collision_aabb(min_point, max_point);
}
void SpatialQueryManager::refresh_spatial_index(CapitalReadView const capitals,
                                                FighterReadView const fighters,
                                                TurretReadView const turrets,
                                                SpinnerReadView const spinners,
                                                std::optional<PlayerSpatialData> const player) {
    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::refresh_spatial_index");
    locations_ = {};
    velocities_ = {};
    locations_[EntityType::CapitalShip] = capitals.entities.view_locations();
    locations_[EntityType::Fighter] = fighters.entities.view_locations();
    locations_[EntityType::Turret] = turrets.entities.view_locations();
    locations_[EntityType::TubeSpinner] = spinners.entities.view_locations();
    velocities_[EntityType::Fighter] = fighters.entities.view_velocities();
    if (player) {
        player_spatial_ = *player;
        auto const& location{player_spatial_.location};
        auto const& velocity{player_spatial_.velocity};
        locations_[EntityType::PlayerShip] = {{&location.X, 1}, {&location.Y, 1}, {&location.Z, 1}};
        velocities_[EntityType::PlayerShip] = {
            {&velocity.X, 1}, {&velocity.Y, 1}, {&velocity.Z, 1}};
    }
    collision_system_.refresh_spatial_index(capitals, fighters, turrets, spinners, player);
}
auto SpatialQueryManager::detect_overlaps(std::span<EntityUniqueId const> const overlap_candidates,
                                          ml::FrameMemoryResource* const scratch_resource)
    -> collision::DetectedOverlapsView {
    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::detect_overlaps");

    return collision_system_.detect_overlaps(overlap_candidates, scratch_resource);
}
void SpatialQueryManager::reset_frame_collision_events() {
    collision_system_.reset_frame_collision_events();
}
auto SpatialQueryManager::get_aabb_overlap_events() const -> collision::AABBOverlapEventsView {
    return collision_system_.get_aabb_overlap_events();
}
auto SpatialQueryManager::get_entity_collision_bounds() const
    -> collision::EntityCellData::ConstView {
    return collision_system_.get_entity_collision_bounds();
}
auto SpatialQueryManager::get_static_collision_bounds() const -> collision::WorldAABBs::ConstView {
    return collision_system_.get_static_collision_bounds();
}

}
