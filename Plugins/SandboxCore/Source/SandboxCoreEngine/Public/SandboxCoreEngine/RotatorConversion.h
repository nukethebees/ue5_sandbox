#pragma once

#include <ioj/rotator_math.h>

#include <CoreMinimal.h>

namespace ioj {
inline auto to_quaternion(FRotator3f const value) noexcept -> Quaternion4f {
    return to_quaternion(Rotator3f{value});
}

inline auto to_unreal(Rotator3f const value) noexcept -> FRotator3f {
    return {value.pitch, value.yaw, value.roll};
}
}
