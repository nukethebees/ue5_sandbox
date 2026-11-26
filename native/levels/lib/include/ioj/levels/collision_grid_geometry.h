#pragma once

#include <ioj/levels/grid_dimensions.h>
#include <ioj/sim/collision/collision_grid_spec.h>
#include <ioj/vector_value_math.h>

namespace ioj::sim::collision {
[[nodiscard]] inline auto is_configured(CollisionGridSpec const spec) noexcept -> bool {
    if (spec.dimensions.x() <= 0 || spec.dimensions.y() <= 0 || spec.dimensions.z() <= 0 ||
        !is_positive_finite(spec.cell_dimensions)) {
        return false;
    }

    return levels::grid_cell_count_fits(
        spec.dimensions.x(), spec.dimensions.y(), spec.dimensions.z());
}

[[nodiscard]] inline auto grid_half_size(CollisionGridSpec const spec) noexcept -> Vector3f {
    return Vector3f{spec.dimensions} * spec.cell_dimensions * 0.5f;
}
}
