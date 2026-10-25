#pragma once

#include <ioj/math_types.h>
#include <ioj/rotator3d.h>

namespace ioj {
struct Transform3d {
    auto operator==(Transform3d const&) const -> bool = default;

    Quaternion4d rotation{};
    Vector3d location{};
    Vector3d scale{1.0, 1.0, 1.0};

    auto rotator() const -> Rotator3d { return to_rotator(rotation); }
    auto forward() const -> Vector3d { return rotate_vector(rotation, {1.0, 0.0, 0.0}); }
    auto right() const -> Vector3d { return rotate_vector(rotation, {0.0, 1.0, 0.0}); }
    auto transform_vector_no_scale(Vector3d const value) const -> Vector3d {
        return rotate_vector(rotation, value);
    }
    auto inverse_transform_vector_no_scale(Vector3d const value) const -> Vector3d {
        return unrotate_vector(rotation, value);
    }
    auto operator*(Transform3d const& parent) const -> Transform3d;
};

}
