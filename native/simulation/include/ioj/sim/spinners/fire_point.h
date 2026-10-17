#pragma once

#include <ioj/rotator_types.h>
#include <ioj/vector_types.h>

namespace ioj::sim::spinners {
struct FirePoint {
    Vector3f location;
    Rotator3f rotation;
};
}
