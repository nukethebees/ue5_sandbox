#pragma once

#include "ioj/sim/collision/collision_scalar_types.h"
#include "ioj/vectors3f.h"
#include <ioj/levels/collision_grid_geometry.h>
#include <ioj/sim/collision/cell_coord.h>
#include <ioj/sim/collision/cell_coord_bounds.h>

#include <cstdint>
#include <limits>
#include <span>

namespace ioj::sim::collision {
inline constexpr float no_trace_hit{std::numeric_limits<float>::infinity()};

/* **************************************** */
// Grid spec
/* **************************************** */

[[nodiscard]] auto calculate_grid_dimensions(Vector3f grid_size, Vector3f cell_size) noexcept
    -> CellCoord;
[[nodiscard]] auto num_cells(CollisionGridSpec spec) noexcept -> GridCellCount;
[[nodiscard]] auto to_index(CollisionGridSpec spec, CellCoord coordinate) noexcept -> CellIndex;
[[nodiscard]] auto to_cell_coord(float value, float cell_dimension, int grid_dimension) noexcept
    -> int;
[[nodiscard]] auto to_cell_coord(CollisionGridSpec spec, Vector3f position) noexcept -> CellCoord;
[[nodiscard]] auto to_max_cell_coord(CollisionGridSpec spec, Vector3f position) noexcept
    -> CellCoord;
[[nodiscard]] auto to_cell_coord_bounds(CollisionGridSpec spec,
                                        Vector3f min_point,
                                        Vector3f max_point) noexcept -> CellCoordBounds;
[[nodiscard]] auto to_cell_min(int coordinate, float cell_dimension, int grid_dimension) noexcept
    -> float;
[[nodiscard]] auto to_cell_min(CollisionGridSpec spec, CellCoord coordinate) noexcept -> Vector3f;
[[nodiscard]] auto to_cell_centre(CollisionGridSpec spec, CellCoord coordinate) noexcept
    -> Vector3f;

/* **************************************** */
// Bounds queries
/* **************************************** */
[[nodiscard]] auto is_cell_coord_in_bounds(CollisionGridSpec spec, CellCoord coordinate) noexcept
    -> bool;
void are_spheres_in_bounds(CollisionGridSpec spec,
                           vectors3f::ConstView centres,
                           float radius,
                           std::span<SphereInBoundsResult> results) noexcept;

/* **************************************** */
// AABB tracing
/* **************************************** */
[[nodiscard]] auto trace_aabb(Vector3f trace_start,
                              Vector3f inverse_trace_delta,
                              Vector3f trace_delta,
                              Vector3f aabb_min,
                              Vector3f aabb_max) noexcept -> float;

// Sweep the moving half extent along the trace against the target bounds.
[[nodiscard]] auto sweep_aabb(Vector3f trace_start,
                              Vector3f inverse_trace_delta,
                              Vector3f trace_delta,
                              Vector3f aabb_min,
                              Vector3f aabb_max,
                              Vector3f moving_half_extent) noexcept -> float;

}
