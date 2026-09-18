#pragma once

#include <ioj/sim/vector_types.h>

#include <CoreMinimal.h>

namespace ml {
inline auto to_native(FVector3f const value) noexcept -> ::ioj::sim::Vector3f {
    return make_vector3f(value.X, value.Y, value.Z);
}

inline auto to_unreal(::ioj::sim::Vector3f const value) noexcept -> FVector3f {
    return {value.X, value.Y, value.Z};
}

}
