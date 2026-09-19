#pragma once

#include <algorithm>
#include <cmath>

namespace ml {
struct Vector3d {
    double x{};
    double y{};
    double z{};

    auto operator==(Vector3d const&) const -> bool = default;
    auto operator+(Vector3d const rhs) const -> Vector3d {
        return {x + rhs.x, y + rhs.y, z + rhs.z};
    }
    auto operator-(Vector3d const rhs) const -> Vector3d {
        return {x - rhs.x, y - rhs.y, z - rhs.z};
    }
    auto operator*(double const scale) const -> Vector3d {
        return {x * scale, y * scale, z * scale};
    }
    auto operator*(Vector3d const rhs) const -> Vector3d {
        return {x * rhs.x, y * rhs.y, z * rhs.z};
    }
    auto operator+=(Vector3d const rhs) -> Vector3d& {
        *this = *this + rhs;
        return *this;
    }
    auto size_squared() const -> double { return x * x + y * y + z * z; }
    auto size() const -> double { return std::sqrt(size_squared()); }
};
inline auto dot(Vector3d const a, Vector3d const b) -> double {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline auto cross(Vector3d const a, Vector3d const b) -> Vector3d {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline auto move_towards(Vector3d const current, Vector3d const target, double const max_delta)
    -> Vector3d {
    auto const delta{target - current};
    auto const distance{delta.size()};
    if (distance <= max_delta) {
        return target;
    }
    if (distance <= 0.0 || max_delta <= 0.0) {
        return current;
    }
    return current + delta * (max_delta / distance);
}
}
