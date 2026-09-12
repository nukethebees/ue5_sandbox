#pragma once

#include <sandbox/simulation/vector_types.h>
#include <sandbox/simulation/vectors3f.h>

#include <CoreMinimal.h>
#include <SandboxCore/soa_vectors_3f.h>

#include <cstddef>

namespace ml {
inline auto to_native(FVector3f const value) noexcept -> simulation::Vector3f {
    return make_vector3f(value.X, value.Y, value.Z);
}

inline auto to_unreal(simulation::Vector3f const value) noexcept -> FVector3f {
    return {value.X, value.Y, value.Z};
}

inline auto to_native(FVectors3f::ConstView const view) noexcept -> simulation::Vectors3fConstView {
    auto const count{static_cast<std::size_t>(view.num())};
    return {{view.xs.GetData(), count}, {view.ys.GetData(), count}, {view.zs.GetData(), count}};
}

inline auto to_native(FVectors3f::View const view) noexcept -> simulation::Vectors3fView {
    auto const count{static_cast<std::size_t>(view.num())};
    return {{view.xs.GetData(), count}, {view.ys.GetData(), count}, {view.zs.GetData(), count}};
}
}
