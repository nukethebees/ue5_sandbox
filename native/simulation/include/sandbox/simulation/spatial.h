#pragma once

namespace ml::simulation {
struct Point3d {
    double x{};
    double y{};
    double z{};
};

[[nodiscard]] auto sample_spherical_shell_point(Point3d centre,
                                                Point3d unit_direction,
                                                float min_distance,
                                                float max_distance,
                                                float unit_squared_distance) noexcept -> Point3d;
}
