#pragma once

#include <ioj/sim/vector_types.h>
#include <ioj/sim/vectors3f.h>

#include <CoreMinimal.h>
#include <SandboxCore/soa_vectors_3f.h>

namespace ml {
inline auto to_native(FVector3f const value) noexcept -> ::ioj::sim::Vector3f {
    return make_vector3f(value.X, value.Y, value.Z);
}

inline auto to_unreal(::ioj::sim::Vector3f const value) noexcept -> FVector3f {
    return {value.X, value.Y, value.Z};
}

inline auto to_native(FVectors3f::ConstView const view) noexcept -> ::ioj::sim::Vectors3fConstView {
    return {view.xs.GetData(), view.ys.GetData(), view.zs.GetData(), view.num()};
}

inline auto to_native(FVectors3f::View const view) noexcept -> ::ioj::sim::Vectors3fView {
    return {view.xs.GetData(), view.ys.GetData(), view.zs.GetData(), view.num()};
}

inline auto to_unreal(::ioj::sim::Vectors3fConstView const view) noexcept -> FVectors3f::ConstView {
    auto const count{view.num()};
    return {{view.xs, count}, {view.ys, count}, {view.zs, count}};
}
}
