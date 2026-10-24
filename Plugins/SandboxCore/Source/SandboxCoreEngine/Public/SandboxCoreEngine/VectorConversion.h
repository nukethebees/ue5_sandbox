#pragma once

#include <ioj/math_types.h>

#include <CoreMinimal.h>

namespace ioj {
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
