#pragma once

#include <ioj/math_types.h>
#include <ioj/rotator_types.h>

namespace ioj::sim {
// Describe a muzzle position and orientation relative to its owning entity.
struct FirePoint {
    Vector3f location;
    Rotator3f rotation;
};
}
