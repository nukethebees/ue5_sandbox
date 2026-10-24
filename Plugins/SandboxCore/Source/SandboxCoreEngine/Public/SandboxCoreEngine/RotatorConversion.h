#pragma once

#include <ioj/rotator_math.h>

#include <CoreMinimal.h>

namespace ioj {
inline auto to_native(FRotator3f const value) noexcept -> ::ioj::Rotator3f {
    return {.pitch = value.Pitch, .yaw = value.Yaw, .roll = value.Roll};
}

inline auto to_native_float(FRotator const value) noexcept -> Rotator3f {
    return to_native(FRotator3f{value});
}

inline auto to_native_float(FQuat const value) noexcept -> Quaternion4f {
    return {{static_cast<float>(value.X),
             static_cast<float>(value.Y),
             static_cast<float>(value.Z),
             static_cast<float>(value.W)}};
}

inline auto to_quaternion(FRotator3f const value) noexcept -> Quaternion4f {
    return to_quaternion(to_native(value));
}

inline auto to_unreal(Rotator3f const value) noexcept -> FRotator3f {
    return {value.pitch, value.yaw, value.roll};
}
}
