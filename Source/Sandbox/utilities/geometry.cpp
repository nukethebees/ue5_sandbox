#include "Sandbox/utilities/geometry.h"

namespace ml {
auto subdivide_arc_into_segments(float starting_angle_deg, float arc_deg, int32 segments)
    -> TArray<float> {
    int32 const n_lines{segments + 1};
    check(segments > 0);
    TArray<float> rots;
    rots.Reserve(n_lines);

    auto const div{arc_deg / static_cast<float>(segments)};
    for (int32 i{0}; i < n_lines; i++) {
        auto const arc_angle_deg{starting_angle_deg + static_cast<float>(i) * div};
        rots.Add(arc_angle_deg);
    }

    return rots;
}
}
