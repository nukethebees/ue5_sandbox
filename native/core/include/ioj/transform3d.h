#pragma once

#include <ioj/math_types.h>
#include <ioj/rotator3d.h>
#include <ioj/vector_types.h>

namespace ioj {
struct Transform3d {
    ioj::Quaternion4d rotation{};
    ioj::Vector3d location{};
    ioj::Vector3d scale{1.0, 1.0, 1.0};

    auto rotator() const -> Rotator3d { return to_rotator(rotation); }
    auto forward() const -> ioj::Vector3d { return rotate_vector(rotation, {1.0, 0.0, 0.0}); }
    auto right() const -> ioj::Vector3d { return rotate_vector(rotation, {0.0, 1.0, 0.0}); }
    auto transform_vector_no_scale(ioj::Vector3d const value) const -> ioj::Vector3d {
        return rotate_vector(rotation, value);
    }
    auto inverse_transform_vector_no_scale(ioj::Vector3d const value) const -> ioj::Vector3d {
        return unrotate_vector(rotation, value);
    }
    auto operator*(Transform3d const& parent) const -> Transform3d;
};
inline auto to_float(ioj::Vector3d const value) -> Vector3f {
    return ioj::make_vector3f(static_cast<float>(value.x()),
                             static_cast<float>(value.y()),
                             static_cast<float>(value.z()));
}
}
