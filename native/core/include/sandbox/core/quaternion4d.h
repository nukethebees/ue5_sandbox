#pragma once

#include <sandbox/core/vector3d.h>

namespace ml {
struct Quaternion4d {
    double x{};
    double y{};
    double z{};
    double w{1.0};

    auto operator*(Quaternion4d const rhs) const -> Quaternion4d {
        return {w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
                w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
                w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
                w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z};
    }
    auto rotate_vector(Vector3d const value) const -> Vector3d {
        Vector3d const axis{x, y, z};
        auto const twice_cross{cross(axis, value) * 2.0};
        return value + twice_cross * w + cross(axis, twice_cross);
    }
    auto unrotate_vector(Vector3d const value) const -> Vector3d {
        return Quaternion4d{-x, -y, -z, w}.rotate_vector(value);
    }
    void normalize() {
        auto const squared{x * x + y * y + z * z + w * w};
        if (squared < 1.e-8) {
            *this = {};
            return;
        }
        auto const scale{1.0 / std::sqrt(squared)};
        x *= scale;
        y *= scale;
        z *= scale;
        w *= scale;
    }
};
}
