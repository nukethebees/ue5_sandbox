#include "Sandbox/utilities/geometry.h"

#include "GameFramework/Actor.h"

namespace ml {
auto get_norm_vector_to_actor(AActor& actor, FVector point) -> FVector {
    auto const fwd{actor.GetActorForwardVector()};
    auto const pos{actor.GetActorLocation()};
    return (point - pos).GetSafeNormal();
}
auto get_angle_to_actor(AActor& actor, FVector point) -> double {
    auto const fwd{actor.GetActorForwardVector()};
    auto const direction_to_point{get_norm_vector_to_actor(actor, point)};

    return get_angle_between_norm_lines(fwd, direction_to_point);
}

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
