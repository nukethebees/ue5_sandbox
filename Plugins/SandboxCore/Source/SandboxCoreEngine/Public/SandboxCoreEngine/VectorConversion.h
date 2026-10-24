#pragma once

#include <ioj/math_types.h>

#include <CoreMinimal.h>

namespace ioj {
inline auto to_native(FVector2D const value) noexcept -> Vector2d {
    return {value.X, value.Y};
}

inline auto to_native(FVector3f const value) noexcept -> ::ioj::Vector3f {
    return Vector3f{value.X, value.Y, value.Z};
}

inline auto to_native(FVector3d const value) noexcept -> Vector3d {
    return {value.X, value.Y, value.Z};
}

inline auto to_native_float(FVector const value) noexcept -> Vector3f {
    return to_native(FVector3f{value});
}

inline auto to_unreal(Vector3f const value) noexcept -> FVector3f {
    return {value.x(), value.y(), value.z()};
}

inline auto to_unreal(Vector3d const value) noexcept -> FVector3d {
    return {value.x(), value.y(), value.z()};
}

inline auto to_unreal(Vector2d const value) noexcept -> FVector2D {
    return {value.x(), value.y()};
}
}
