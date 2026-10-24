#pragma once

#include <ioj/transform3d.h>
#include <ioj/vector_value_math.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <CoreMinimal.h>

namespace ioj {
inline auto to_native(FTransform const& value) -> ::ioj::Transform3d {
    auto const rotation{value.GetRotation()};
    return {{rotation.X, rotation.Y, rotation.Z, rotation.W},
            to_native(value.GetLocation()),
            to_native(value.GetScale3D())};
}
inline auto to_unreal(Transform3d const& value) -> FTransform {
    return {FQuat{value.rotation.x(), value.rotation.y(), value.rotation.z(), value.rotation.w()},
            to_unreal(value.location),
            to_unreal(value.scale)};
}
}
