#include "sandbox/simulation/collision_grid.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ml::simulation::collision {
namespace {
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

auto half_grid_size(GridGeometry const geometry) noexcept -> Vec3f {
    return make_vector3f(
        static_cast<float>(geometry.dimensions.x) * geometry.cell_dimensions.X * 0.5f,
        static_cast<float>(geometry.dimensions.y) * geometry.cell_dimensions.Y * 0.5f,
        static_cast<float>(geometry.dimensions.z) * geometry.cell_dimensions.Z * 0.5f);
}

auto clip_segment(Vec3f const start,
                  Vec3f const end,
                  Vec3f const bounds_min,
                  Vec3f const bounds_max_inside,
                  Vec3f& clipped_start,
                  Vec3f& clipped_end) noexcept -> bool {
    auto const delta{make_vector3f(end.X - start.X, end.Y - start.Y, end.Z - start.Z)};
    float entry_t{};
    float exit_t{1.0f};

    for (std::size_t axis{}; axis < axis_count; ++axis) {
        auto const axis_delta{delta.Elements[axis]};
        if (axis_delta == 0.0f) {
            if (start.Elements[axis] < bounds_min.Elements[axis] ||
                start.Elements[axis] > bounds_max_inside.Elements[axis]) {
                return false;
            }
            continue;
        }

        auto axis_entry_t{(bounds_min.Elements[axis] - start.Elements[axis]) / axis_delta};
        auto axis_exit_t{(bounds_max_inside.Elements[axis] - start.Elements[axis]) / axis_delta};
        if (axis_entry_t > axis_exit_t) {
            std::swap(axis_entry_t, axis_exit_t);
        }

        entry_t = std::max(entry_t, axis_entry_t);
        exit_t = std::min(exit_t, axis_exit_t);
        if (entry_t > exit_t) {
            return false;
        }
    }

    for (std::size_t axis{}; axis < axis_count; ++axis) {
        clipped_start.Elements[axis] =
            std::clamp(start.Elements[axis] + delta.Elements[axis] * entry_t,
                       bounds_min.Elements[axis],
                       bounds_max_inside.Elements[axis]);
        clipped_end.Elements[axis] =
            std::clamp(start.Elements[axis] + delta.Elements[axis] * exit_t,
                       bounds_min.Elements[axis],
                       bounds_max_inside.Elements[axis]);
    }
    return true;
}
}

auto CellCoord::operator[](std::size_t const index) noexcept -> int& {
    if (index == 0) {
        return x;
    }
    if (index == 1) {
        return y;
    }
    return z;
}
auto CellCoord::operator[](std::size_t const index) const noexcept -> int {
    if (index == 0) {
        return x;
    }
    if (index == 1) {
        return y;
    }
    return z;
}

auto is_configured(GridGeometry const geometry) noexcept -> bool {
    if (geometry.dimensions.x <= 0 || geometry.dimensions.y <= 0 || geometry.dimensions.z <= 0 ||
        geometry.cell_dimensions.X <= 0.0f || geometry.cell_dimensions.Y <= 0.0f ||
        geometry.cell_dimensions.Z <= 0.0f) {
        return false;
    }

    auto const xy_cell_count{static_cast<long long>(geometry.dimensions.x) * geometry.dimensions.y};
    return xy_cell_count <= std::numeric_limits<int>::max() / geometry.dimensions.z;
}

auto to_cell_coord(float const value, float const cell_dimension, int const grid_dimension) noexcept
    -> int {
    auto const half_extent{static_cast<float>(grid_dimension) * cell_dimension * 0.5f};
    return to_cell(value, cell_dimension, half_extent);
}

auto to_cell_min(int const coordinate,
                 float const cell_dimension,
                 int const grid_dimension) noexcept -> float {
    auto const half_extent{static_cast<float>(grid_dimension) * cell_dimension * 0.5f};
    return static_cast<float>(coordinate) * cell_dimension - half_extent;
}

auto to_cell_coord(GridGeometry const geometry, Vec3f const position) noexcept -> CellCoord {
    auto const half_size{half_grid_size(geometry)};
    return {
        to_cell(position.X, geometry.cell_dimensions.X, half_size.X),
        to_cell(position.Y, geometry.cell_dimensions.Y, half_size.Y),
        to_cell(position.Z, geometry.cell_dimensions.Z, half_size.Z),
    };
}

auto to_max_cell_coord(GridGeometry const geometry, Vec3f const position) noexcept -> CellCoord {
    auto const half_size{half_grid_size(geometry)};
    return {
        to_closed_max_cell(
            position.X, geometry.cell_dimensions.X, geometry.dimensions.x, half_size.X),
        to_closed_max_cell(
            position.Y, geometry.cell_dimensions.Y, geometry.dimensions.y, half_size.Y),
        to_closed_max_cell(
            position.Z, geometry.cell_dimensions.Z, geometry.dimensions.z, half_size.Z),
    };
}

auto to_cell_coord_bounds(GridGeometry const geometry,
                          Vec3f const min_point,
                          Vec3f const max_point) noexcept -> CellCoordBounds {
    return {to_cell_coord(geometry, min_point), to_max_cell_coord(geometry, max_point)};
}

auto to_cell_min(GridGeometry const geometry, CellCoord const coordinate) noexcept -> Vec3f {
    auto const half_size{half_grid_size(geometry)};
    return make_vector3f(
        static_cast<float>(coordinate.x) * geometry.cell_dimensions.X - half_size.X,
        static_cast<float>(coordinate.y) * geometry.cell_dimensions.Y - half_size.Y,
        static_cast<float>(coordinate.z) * geometry.cell_dimensions.Z - half_size.Z);
}

auto to_cell_centre(GridGeometry const geometry, CellCoord const coordinate) noexcept -> Vec3f {
    auto result{to_cell_min(geometry, coordinate)};
    result.X += geometry.cell_dimensions.X * 0.5f;
    result.Y += geometry.cell_dimensions.Y * 0.5f;
    result.Z += geometry.cell_dimensions.Z * 0.5f;
    return result;
}

auto is_cell_coord_in_bounds(GridGeometry const geometry, CellCoord const coordinate) noexcept
    -> bool {
    return coordinate.x >= 0 && coordinate.x < geometry.dimensions.x && coordinate.y >= 0 &&
           coordinate.y < geometry.dimensions.y && coordinate.z >= 0 &&
           coordinate.z < geometry.dimensions.z;
}

auto trace_aabb(Vec3f const trace_start,
                Vec3f const inverse_trace_delta,
                Vec3f const trace_delta,
                Vec3f const aabb_min,
                Vec3f const aabb_max,
                Vec3f const expansion) noexcept -> float {
    constexpr auto no_hit{std::numeric_limits<float>::infinity()};
    float minimum_t{};
    float maximum_t{1.0f};

    for (std::size_t axis{}; axis < axis_count; ++axis) {
        auto const slab_min{aabb_min.Elements[axis] - expansion.Elements[axis]};
        auto const slab_max{aabb_max.Elements[axis] + expansion.Elements[axis]};
        auto const start{trace_start.Elements[axis]};
        auto const axis_delta{trace_delta.Elements[axis]};
        if (axis_delta == 0.0f) {
            if (start < slab_min || start > slab_max) {
                return no_hit;
            }
            continue;
        }

        auto first_t{(slab_min - start) * inverse_trace_delta.Elements[axis]};
        auto second_t{(slab_max - start) * inverse_trace_delta.Elements[axis]};
        if (first_t > second_t) {
            std::swap(first_t, second_t);
        }
        minimum_t = std::max(minimum_t, first_t);
        maximum_t = std::min(maximum_t, second_t);
        if (minimum_t > maximum_t) {
            return no_hit;
        }
    }
    return minimum_t;
}

auto GridTraversal::create(GridGeometry const geometry,
                           Vec3f const start,
                           Vec3f const end,
                           GridTraversal& result) noexcept -> bool {
    if (!is_configured(geometry)) {
        return false;
    }

    auto const half_size{half_grid_size(geometry)};
    auto const bounds_min{make_vector3f(-half_size.X, -half_size.Y, -half_size.Z)};
    Vec3f bounds_max_inside{half_size};
    for (std::size_t axis{}; axis < axis_count; ++axis) {
        bounds_max_inside.Elements[axis] =
            std::nextafter(bounds_max_inside.Elements[axis], bounds_min.Elements[axis]);
    }

    Vec3f clipped_start;
    Vec3f clipped_end;
    if (!clip_segment(start, end, bounds_min, bounds_max_inside, clipped_start, clipped_end)) {
        return false;
    }

    auto clamp_cell = [geometry](Vec3f const point) {
        auto result{to_cell_coord(geometry, point)};
        result.x = std::clamp(result.x, 0, geometry.dimensions.x - 1);
        result.y = std::clamp(result.y, 0, geometry.dimensions.y - 1);
        result.z = std::clamp(result.z, 0, geometry.dimensions.z - 1);
        return result;
    };

    result.current_cell_ = clamp_cell(clipped_start);
    result.end_cell_ = clamp_cell(clipped_end);
    auto const cell_min{to_cell_min(geometry, result.current_cell_)};
    auto const delta{make_vector3f(clipped_end.X - clipped_start.X,
                                   clipped_end.Y - clipped_start.Y,
                                   clipped_end.Z - clipped_start.Z)};
    for (std::size_t axis{}; axis < axis_count; ++axis) {
        if (delta.Elements[axis] == 0.0f) {
            result.steps_[axis] = 0;
            result.next_t_[axis] = std::numeric_limits<float>::max();
            result.t_deltas_[axis] = std::numeric_limits<float>::max();
            continue;
        }

        result.steps_[axis] = delta.Elements[axis] > 0.0f ? 1 : -1;
        auto const next_boundary{
            cell_min.Elements[axis] +
            (result.steps_[axis] > 0 ? geometry.cell_dimensions.Elements[axis] : 0.0f)};
        result.next_t_.Elements[axis] =
            (next_boundary - clipped_start.Elements[axis]) / delta.Elements[axis];
        result.t_deltas_.Elements[axis] =
            geometry.cell_dimensions.Elements[axis] / std::abs(delta.Elements[axis]);
    }
    return true;
}

auto GridTraversal::current_cell() const noexcept -> CellCoord {
    return current_cell_;
}

auto GridTraversal::advance() noexcept -> bool {
    if (current_cell_ == end_cell_) {
        return false;
    }
    auto const next_t{std::min(next_t_.X, std::min(next_t_.Y, next_t_.Z))};
    if (next_t > 1.0f) {
        return false;
    }
    for (std::size_t axis{}; axis < axis_count; ++axis) {
        if (next_t_.Elements[axis] == next_t) {
            current_cell_[axis] += steps_[axis];
            next_t_.Elements[axis] += t_deltas_.Elements[axis];
        }
    }
    return true;
}
}
