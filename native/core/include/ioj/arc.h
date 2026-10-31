#pragma once

#include <cstdint>
#include <vector>

namespace ioj {
// Require a positive segment count; the result includes both arc endpoints.
auto subdivide_arc_into_segments(float starting_angle_degrees,
                                 float arc_degrees,
                                 std::int32_t segments) -> std::vector<float>;
}
