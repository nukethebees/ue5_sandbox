#pragma once

#include <ioj/rotator_types.h>

#include <CoreMinimal.h>

namespace ioj {
inline auto to_native(FRotator3f const value) noexcept -> ::ioj::Rotator3f {
    return {.pitch = value.Pitch, .yaw = value.Yaw, .roll = value.Roll};
}

inline auto to_unreal(Rotator3f const value) noexcept -> FRotator3f {
    return {value.pitch, value.yaw, value.roll};
}
}
