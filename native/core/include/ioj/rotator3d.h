#pragma once

#include <ioj/quaternion.h>
#include <ioj/rotator_types.h>

namespace ioj {
auto to_quaternion(Rotator3d rotation) noexcept -> Quaternion4d;
auto to_rotator(Quaternion4d q) noexcept -> Rotator3d;
}
