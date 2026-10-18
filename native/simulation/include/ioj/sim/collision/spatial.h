#pragma once

#include <ioj/vector_value_math.h>

namespace ioj::sim {
using Point3d = Vector3d;

[[nodiscard]] auto sample_spherical_shell_point(Point3d centre,
                                                Point3d unit_direction,
                                                float min_distance,
                                                float max_distance,
                                                float unit_squared_distance) noexcept -> Point3d;
}
