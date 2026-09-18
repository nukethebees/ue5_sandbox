#pragma once

#include <ioj/sim/rotator_types.h>

#include <CoreMinimal.h>

namespace ml {
inline auto to_native(FRotator3f const value) noexcept -> ::ioj::sim::Rotator3f {
    return {.pitch = value.Pitch, .yaw = value.Yaw, .roll = value.Roll};
}

inline auto to_unreal(::ioj::sim::Rotator3f const value) noexcept -> FRotator3f {
    return {value.pitch, value.yaw, value.roll};
}

} // namespace ml
