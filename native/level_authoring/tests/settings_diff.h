#pragma once

#include <ioj/frequency.h>
#include <ioj/sim/lasers/fire_point.h>
#include <ioj/testing/math_diff.h>

namespace ioj {
inline void diff_members(Frequency const expected, Frequency const actual, DiffContext& context) {
    context.diff("hz", expected.get_hz(), actual.get_hz());
}
}

namespace ioj::sim {
inline void diff_members(FirePoint const& expected, FirePoint const& actual, DiffContext& context) {
    context.diff("location", expected.location, actual.location);
    context.diff("rotation", expected.rotation, actual.rotation);
}
}
