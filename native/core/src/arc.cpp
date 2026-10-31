#include <ioj/arc.h>

#include <cassert>

namespace ioj {
auto subdivide_arc_into_segments(float const starting_angle_degrees,
                                 float const arc_degrees,
                                 std::int32_t const segments) -> std::vector<float> {
    assert(segments > 0);
    auto const count{static_cast<std::size_t>(segments) + 1};
    std::vector<float> angles;
    angles.reserve(count);
    auto const step{arc_degrees / static_cast<float>(segments)};
    for (std::size_t index{}; index < count; ++index) {
        angles.push_back(starting_angle_degrees + static_cast<float>(index) * step);
    }
    return angles;
}
}
