#pragma once
#include <ioj/math_types.h>

namespace ioj::native_soa_fixture {
inline auto make_vector(float x, float y, float z) -> Vector3f {
    return {x, y, z};
}
}
