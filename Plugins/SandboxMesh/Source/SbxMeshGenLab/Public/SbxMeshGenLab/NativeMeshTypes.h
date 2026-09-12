#pragma once

#include <mesh_gen/MeshGeneration.h>

#include "CoreMinimal.h"

namespace SandboxMesh {

[[nodiscard]] inline auto to_native(FName const value) -> std::string {
    return TCHAR_TO_UTF8(*value.ToString());
}

[[nodiscard]] inline auto to_native(FVector3f const value) -> mesh_gen::Vec3f {
    return {value.X, value.Y, value.Z};
}

[[nodiscard]] inline auto to_native(FVector const value) -> mesh_gen::Vec3f {
    return {static_cast<float>(value.X), static_cast<float>(value.Y), static_cast<float>(value.Z)};
}

[[nodiscard]] inline auto to_native(FRotator3f const value) -> mesh_gen::Rotatorf {
    return {value.Pitch, value.Yaw, value.Roll};
}

[[nodiscard]] inline auto to_native(FRotator const value) -> mesh_gen::Rotatorf {
    return {static_cast<float>(value.Pitch),
            static_cast<float>(value.Yaw),
            static_cast<float>(value.Roll)};
}

[[nodiscard]] inline auto to_unreal(mesh_gen::Vec3f const value) -> FVector {
    return {value.x, value.y, value.z};
}

[[nodiscard]] inline auto to_unreal_float(mesh_gen::Vec3f const value) -> FVector3f {
    return {value.x, value.y, value.z};
}

[[nodiscard]] inline auto to_unreal(mesh_gen::Vec2f const value) -> FVector2f {
    return {value.x, value.y};
}

[[nodiscard]] inline auto to_unreal(mesh_gen::Rotatorf const value) -> FRotator {
    return {value.pitch, value.yaw, value.roll};
}

[[nodiscard]] inline auto to_unreal_name(std::string const& value) -> FName {
    return FName{UTF8_TO_TCHAR(value.c_str())};
}

}
