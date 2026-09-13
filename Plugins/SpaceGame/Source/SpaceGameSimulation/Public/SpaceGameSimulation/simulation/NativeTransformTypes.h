#pragma once

#include <CoreMinimal.h>
#include <sandbox/core/vector2d.h>
#include <sandbox/simulation/transform3d.h>

namespace ml {
inline auto to_native(FVector const value) -> Vector3d {
    return {value.X, value.Y, value.Z};
}
inline auto to_native(FVector2D const value) -> Vector2d {
    return {value.X, value.Y};
}
inline auto to_unreal(Vector3d const value) -> FVector {
    return {value.x, value.y, value.z};
}
inline auto to_unreal(Vector2d const value) -> FVector2D {
    return {value.x, value.y};
}
inline auto to_native(FTransform const& value) -> simulation::Transform3d {
    auto const rotation{value.GetRotation()};
    return {{rotation.X, rotation.Y, rotation.Z, rotation.W},
            to_native(value.GetLocation()),
            to_native(value.GetScale3D())};
}
inline auto to_unreal(simulation::Transform3d const& value) -> FTransform {
    return {FQuat{value.rotation.x, value.rotation.y, value.rotation.z, value.rotation.w},
            to_unreal(value.location),
            to_unreal(value.scale)};
}
}
