#include <ioj/sim/collision/grid_traversal.h>

#include <ioj/sim/collision/collision_grid.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ioj::sim::collision {
namespace {
auto clip_segment(Vector3f const start,
                  Vector3f const end,
                  Vector3f const bounds_min,
                  Vector3f const bounds_max_inside,
                  Vector3f& clipped_start,
                  Vector3f& clipped_end) noexcept -> bool {
    auto const delta{ml::make_vector3f(end.X - start.X, end.Y - start.Y, end.Z - start.Z)};
    float entry_t{};
    float exit_t{1.0f};

    for (std::size_t axis{}; axis < 3; ++axis) {
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

    for (std::size_t axis{}; axis < 3; ++axis) {
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

/* **************************************** */
// Grid traversal
/* **************************************** */
auto GridTraversal::create(GridGeometry const geometry,
                           Vector3f const start,
                           Vector3f const end,
                           GridTraversal& result) noexcept -> bool {
    if (!is_configured(geometry)) {
        return false;
    }

    auto const half_size{-to_cell_min(geometry, {})};
    auto const bounds_min{ml::make_vector3f(-half_size.X, -half_size.Y, -half_size.Z)};
    Vector3f bounds_max_inside{half_size};
    for (std::size_t axis{}; axis < 3; ++axis) {
        bounds_max_inside.Elements[axis] =
            std::nextafter(bounds_max_inside.Elements[axis], bounds_min.Elements[axis]);
    }

    Vector3f clipped_start;
    Vector3f clipped_end;
    if (!clip_segment(start, end, bounds_min, bounds_max_inside, clipped_start, clipped_end)) {
        return false;
    }

    auto clamp_cell = [geometry](Vector3f const point) {
        return to_cell_coord(geometry, point).clamped({}, geometry.dimensions - 1);
    };

    result.current_cell_ = clamp_cell(clipped_start);
    result.end_cell_ = clamp_cell(clipped_end);
    auto const cell_min{to_cell_min(geometry, result.current_cell_)};
    auto const delta{ml::make_vector3f(clipped_end.X - clipped_start.X,
                                       clipped_end.Y - clipped_start.Y,
                                       clipped_end.Z - clipped_start.Z)};
    for (std::size_t axis{}; axis < 3; ++axis) {
        if (delta.Elements[axis] == 0.0f) {
            result.steps_[axis] = 0;
            result.next_t_.Elements[axis] = no_axis_crossing_t;
            result.t_deltas_.Elements[axis] = no_axis_crossing_t;
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
    auto const next_t{std::min({next_t_.X, next_t_.Y, next_t_.Z})};
    if (next_t > 1.0f) {
        return false;
    }
    for (std::size_t axis{}; axis < 3; ++axis) {
        if (next_t_.Elements[axis] == next_t) {
            current_cell_[axis] += steps_[axis];
            next_t_.Elements[axis] += t_deltas_.Elements[axis];
        }
    }
    return true;
}
}
