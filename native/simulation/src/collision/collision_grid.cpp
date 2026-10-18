#include "ioj/sim/collision/collision_grid.h"

#include "ioj/vectors3f.h"
#include <ioj/levels/grid_dimensions.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

namespace ioj::sim::collision {
namespace {
/* **************************************** */
// Internal helpers
/* **************************************** */
constexpr std::size_t axis_count{3};

auto to_cell(float const value, float const cell_dimension, float const half_grid_extent) noexcept
    -> int {
    return static_cast<int>(std::floor((value + half_grid_extent) / cell_dimension));
}
auto to_closed_max_cell(float const value,
                        float const cell_dimension,
                        int const grid_dimension,
                        float const half_grid_extent) noexcept -> int {
    if (value == half_grid_extent) {
        return grid_dimension - 1;
    }
    return to_cell(value, cell_dimension, half_grid_extent);
}
auto grid_half_size(GridGeometry const geometry) noexcept -> Vector3f {
    return ioj::make_vector3f(
        static_cast<float>(geometry.dimensions.x()) * geometry.cell_dimensions.x() * 0.5f,
        static_cast<float>(geometry.dimensions.y()) * geometry.cell_dimensions.y() * 0.5f,
        static_cast<float>(geometry.dimensions.z()) * geometry.cell_dimensions.z() * 0.5f);
}

}

/* **************************************** */
// Grid geometry
/* **************************************** */
auto is_configured(GridGeometry const geometry) noexcept -> bool {
    if (geometry.dimensions.x() <= 0 || geometry.dimensions.y() <= 0 ||
        geometry.dimensions.z() <= 0 || !std::isfinite(geometry.cell_dimensions.x()) ||
        !std::isfinite(geometry.cell_dimensions.y()) ||
        !std::isfinite(geometry.cell_dimensions.z()) || geometry.cell_dimensions.x() <= 0.0f ||
        geometry.cell_dimensions.y() <= 0.0f || geometry.cell_dimensions.z() <= 0.0f) {
        return false;
    }

    return ioj::levels::grid_cell_count_fits(
        geometry.dimensions.x(), geometry.dimensions.y(), geometry.dimensions.z());
}
auto calculate_grid_dimensions(Vector3f const grid_size, Vector3f const cell_size) noexcept
    -> CellCoord {
    return {ioj::levels::grid_axis_count(grid_size.x(), cell_size.x()),
            ioj::levels::grid_axis_count(grid_size.y(), cell_size.y()),
            ioj::levels::grid_axis_count(grid_size.z(), cell_size.z())};
}
auto num_cells(GridGeometry const geometry) noexcept -> GridCellCount {
    return geometry.dimensions.x() * geometry.dimensions.y() * geometry.dimensions.z();
}
auto to_index(GridGeometry const geometry, CellCoord const coordinate) noexcept -> CellIndex {
    return coordinate.x() + coordinate.y() * geometry.dimensions.x() +
           coordinate.z() * geometry.dimensions.x() * geometry.dimensions.y();
}
auto to_cell_coord(float const value, float const cell_dimension, int const grid_dimension) noexcept
    -> int {
    auto const half_extent{static_cast<float>(grid_dimension) * cell_dimension * 0.5f};
    return to_cell(value, cell_dimension, half_extent);
}
auto to_cell_coord(GridGeometry const geometry, Vector3f const position) noexcept -> CellCoord {
    auto const half_size{grid_half_size(geometry)};
    return {
        to_cell(position.x(), geometry.cell_dimensions.x(), half_size.x()),
        to_cell(position.y(), geometry.cell_dimensions.y(), half_size.y()),
        to_cell(position.z(), geometry.cell_dimensions.z(), half_size.z()),
    };
}
auto to_max_cell_coord(GridGeometry const geometry, Vector3f const position) noexcept -> CellCoord {
    auto const half_size{grid_half_size(geometry)};
    return {
        to_closed_max_cell(
            position.x(), geometry.cell_dimensions.x(), geometry.dimensions.x(), half_size.x()),
        to_closed_max_cell(
            position.y(), geometry.cell_dimensions.y(), geometry.dimensions.y(), half_size.y()),
        to_closed_max_cell(
            position.z(), geometry.cell_dimensions.z(), geometry.dimensions.z(), half_size.z()),
    };
}
auto to_cell_coord_bounds(GridGeometry const geometry,
                          Vector3f const min_point,
                          Vector3f const max_point) noexcept -> CellCoordBounds {
    return {to_cell_coord(geometry, min_point), to_max_cell_coord(geometry, max_point)};
}
auto to_cell_min(int const coordinate,
                 float const cell_dimension,
                 int const grid_dimension) noexcept -> float {
    auto const half_extent{static_cast<float>(grid_dimension) * cell_dimension * 0.5f};
    return static_cast<float>(coordinate) * cell_dimension - half_extent;
}
auto to_cell_min(GridGeometry const geometry, CellCoord const coordinate) noexcept -> Vector3f {
    auto const half_size{grid_half_size(geometry)};
    return ioj::make_vector3f(
        static_cast<float>(coordinate.x()) * geometry.cell_dimensions.x() - half_size.x(),
        static_cast<float>(coordinate.y()) * geometry.cell_dimensions.y() - half_size.y(),
        static_cast<float>(coordinate.z()) * geometry.cell_dimensions.z() - half_size.z());
}
auto to_cell_centre(GridGeometry const geometry, CellCoord const coordinate) noexcept -> Vector3f {
    auto result{to_cell_min(geometry, coordinate)};
    result.x() += geometry.cell_dimensions.x() * 0.5f;
    result.y() += geometry.cell_dimensions.y() * 0.5f;
    result.z() += geometry.cell_dimensions.z() * 0.5f;
    return result;
}

/* **************************************** */
// Bounds queries
/* **************************************** */
auto is_cell_coord_in_bounds(GridGeometry const geometry, CellCoord const coordinate) noexcept
    -> bool {
    return coordinate.x() >= 0 && coordinate.x() < geometry.dimensions.x() && coordinate.y() >= 0 &&
           coordinate.y() < geometry.dimensions.y() && coordinate.z() >= 0 &&
           coordinate.z() < geometry.dimensions.z();
}
void are_spheres_in_bounds(GridGeometry const geometry,
                           Vectors3fConstView const centres,
                           float const radius,
                           std::span<SphereInBoundsResult> const results) noexcept {
    assert(static_cast<std::size_t>(centres.num()) == results.size());
    assert(std::isfinite(radius));
    assert(radius >= 0.0f);

    auto const half_size{grid_half_size(geometry)};
    auto const min_x{-half_size.x() + radius};
    auto const min_y{-half_size.y() + radius};
    auto const min_z{-half_size.z() + radius};
    auto const max_x{half_size.x() - radius};
    auto const max_y{half_size.y() - radius};
    auto const max_z{half_size.z() - radius};

    auto const count{static_cast<std::size_t>(centres.num())};
    auto const xs{centres.xs()};
    auto const ys{centres.ys()};
    auto const zs{centres.zs()};
    for (std::size_t index{}; index < count; ++index) {
        auto const is_in_bounds{xs[index] >= min_x && xs[index] <= max_x && ys[index] >= min_y &&
                                ys[index] <= max_y && zs[index] >= min_z && zs[index] <= max_z};
        results[index] = static_cast<SphereInBoundsResult>(is_in_bounds);
    }
}

/* **************************************** */
// AABB tracing
/* **************************************** */
auto trace_aabb(Vector3f const trace_start,
                Vector3f const inverse_trace_delta,
                Vector3f const trace_delta,
                Vector3f const aabb_min,
                Vector3f const aabb_max) noexcept -> float {
    float minimum_t{};
    float maximum_t{1.0f};

    for (std::size_t axis{}; axis < axis_count; ++axis) {
        auto const slab_min{aabb_min[axis]};
        auto const slab_max{aabb_max[axis]};
        auto const start{trace_start[axis]};
        auto const axis_delta{trace_delta[axis]};
        if (axis_delta == 0.0f) {
            if (start < slab_min || start > slab_max) {
                return no_trace_hit;
            }
            continue;
        }

        auto first_t{(slab_min - start) * inverse_trace_delta[axis]};
        auto second_t{(slab_max - start) * inverse_trace_delta[axis]};
        if (first_t > second_t) {
            std::swap(first_t, second_t);
        }
        minimum_t = std::max(minimum_t, first_t);
        maximum_t = std::min(maximum_t, second_t);
        if (minimum_t > maximum_t) {
            return no_trace_hit;
        }
    }
    return minimum_t;
}

auto sweep_aabb(Vector3f const trace_start,
                Vector3f const inverse_trace_delta,
                Vector3f const trace_delta,
                Vector3f const aabb_min,
                Vector3f const aabb_max,
                Vector3f const moving_half_extent) noexcept -> float {
    return trace_aabb(trace_start,
                      inverse_trace_delta,
                      trace_delta,
                      aabb_min - moving_half_extent,
                      aabb_max + moving_half_extent);
}

}
