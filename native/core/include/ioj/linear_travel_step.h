#pragma once

#include <ioj/math_types.h>

namespace ioj {
struct LinearTravelStep {
    Vector3d location{};
    bool arrived{};
};
}
