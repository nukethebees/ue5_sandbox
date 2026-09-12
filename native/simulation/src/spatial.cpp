#include "sandbox/simulation/spatial.h"

#include <algorithm>
#include <cmath>

namespace ml::simulation {
auto sample_spherical_shell_point(Point3d const centre,
                                  Point3d const unit_direction,
                                  float const min_distance,
                                  float const max_distance,
                                  float const unit_squared_distance) noexcept -> Point3d {
    auto const alpha{std::clamp(unit_squared_distance, 0.0f, 1.0f)};
    auto const minimum_squared{min_distance * min_distance};
    auto const maximum_squared{max_distance * max_distance};
    auto const distance{std::sqrt(minimum_squared + (maximum_squared - minimum_squared) * alpha)};

    return {
        centre.x + unit_direction.x * distance,
        centre.y + unit_direction.y * distance,
        centre.z + unit_direction.z * distance,
    };
}
}
