#pragma once

#include <ioj/quaternion.h>
#include <ioj/vector_value_math.h>

#include <type_traits>

namespace ioj {

static_assert(sizeof(Vector3f) == sizeof(float) * 3);
static_assert(alignof(Vector3f) == alignof(float));
static_assert(std::is_trivially_copyable_v<Vector3f>);
static_assert(std::is_standard_layout_v<Vector3f>);
static_assert(sizeof(Quaternion4f) == 16 && alignof(Quaternion4f) == 16);

}
