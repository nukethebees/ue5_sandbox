#pragma once

#include <ioj/math_types.h>

namespace ioj {
struct LinearTravelState {
    Vector3d start{};
    Vector3d end{};
    double pause_remaining{};
    bool towards_end{true};
};
}
