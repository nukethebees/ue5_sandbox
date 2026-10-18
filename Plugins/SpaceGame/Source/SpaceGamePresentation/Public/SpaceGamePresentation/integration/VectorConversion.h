#pragma once

#include <ioj/math_types.h>

#include <CoreMinimal.h>

namespace ioj {
inline auto to_native(FVector3f const value) noexcept -> ::ioj::Vector3f {
    return ioj::Vector3f{value.X, value.Y, value.Z};
}

inline auto to_unreal(::ioj::Vector3f const value) noexcept -> FVector3f {
    return {value.x(), value.y(), value.z()};
}
}
