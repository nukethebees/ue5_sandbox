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
    auto const delta{ioj::Vector3f{end.x() - start.x(), end.y() - start.y(), end.z() - start.z()}};
    float entry_t{};
    float exit_t{1.0f};

    for (std::size_t axis{}; axis < 3; ++axis) {
        auto const axis_delta{delta[axis]};
        if (axis_delta == 0.0f) {
            if (start[axis] < bounds_min[axis] || start[axis] > bounds_max_inside[axis]) {
                return false;
            }
            continue;
        }

        auto axis_entry_t{(bounds_min[axis] - start[axis]) / axis_delta};
        auto axis_exit_t{(bounds_max_inside[axis] - start[axis]) / axis_delta};
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
        clipped_start[axis] = std::clamp(
            start[axis] + delta[axis] * entry_t, bounds_min[axis], bounds_max_inside[axis]);
        clipped_end[axis] = std::clamp(
            start[axis] + delta[axis] * exit_t, bounds_min[axis], bounds_max_inside[axis]);
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
    auto const bounds_min{ioj::Vector3f{-half_size.x(), -half_size.y(), -half_size.z()}};
    Vector3f bounds_max_inside{half_size};
    for (std::size_t axis{}; axis < 3; ++axis) {
        bounds_max_inside[axis] = std::nextafter(bounds_max_inside[axis], bounds_min[axis]);
    }

    Vector3f clipped_start;
    Vector3f clipped_end;
    if (!clip_segment(start, end, bounds_min, bounds_max_inside, clipped_start, clipped_end)) {
        return false;
    }

    auto clamp_cell = [geometry](Vector3f const point) {
        return clamped(to_cell_coord(geometry, point), CellCoord{}, geometry.dimensions - 1);
    };

    result.current_cell_ = clamp_cell(clipped_start);
    result.end_cell_ = clamp_cell(clipped_end);
    auto const cell_min{to_cell_min(geometry, result.current_cell_)};
    auto const delta{ioj::Vector3f{clipped_end.x() - clipped_start.x(),
                                   clipped_end.y() - clipped_start.y(),
                                   clipped_end.z() - clipped_start.z()}};
    for (std::size_t axis{}; axis < 3; ++axis) {
        if (delta[axis] == 0.0f) {
            result.steps_[axis] = 0;
            result.next_t_[axis] = no_axis_crossing_t;
            result.t_deltas_[axis] = no_axis_crossing_t;
            continue;
        }

        result.steps_[axis] = delta[axis] > 0.0f ? 1 : -1;
        auto const next_boundary{cell_min[axis] +
                                 (result.steps_[axis] > 0 ? geometry.cell_dimensions[axis] : 0.0f)};
        result.next_t_[axis] = (next_boundary - clipped_start[axis]) / delta[axis];
        result.t_deltas_[axis] = geometry.cell_dimensions[axis] / std::abs(delta[axis]);
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
    auto const next_t{std::min({next_t_.x(), next_t_.y(), next_t_.z()})};
    if (next_t > 1.0f) {
        return false;
    }
    for (std::size_t axis{}; axis < 3; ++axis) {
        if (next_t_[axis] == next_t) {
            current_cell_[axis] += steps_[axis];
            next_t_[axis] += t_deltas_[axis];
        }
    }
    return true;
}
}
