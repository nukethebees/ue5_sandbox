#pragma once

#include <sandbox/core/math_types.h>
#include <sandbox/simulation/rotator3d.h>

namespace ml::simulation {
struct Transform3d {
    ml::Quaternion4d rotation{};
    ml::Vector3d location{};
    ml::Vector3d scale{1.0, 1.0, 1.0};

    auto rotator() const -> Rotator3d { return to_rotator(rotation); }
    auto forward() const -> ml::Vector3d { return rotation.rotate_vector({1.0, 0.0, 0.0}); }
    auto right() const -> ml::Vector3d { return rotation.rotate_vector({0.0, 1.0, 0.0}); }
    auto transform_vector_no_scale(ml::Vector3d const value) const -> ml::Vector3d {
        return rotation.rotate_vector(value);
    }
    auto inverse_transform_vector_no_scale(ml::Vector3d const value) const -> ml::Vector3d {
        return rotation.unrotate_vector(value);
    }
    auto operator*(Transform3d const& parent) const -> Transform3d;
};
inline auto to_float(ml::Vector3d const value) -> Vector3f {
    return ml::make_vector3f(
        static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z));
}
}
