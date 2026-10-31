#include "SandboxGameShared/utilities/geometry.h"

#include "GameFramework/Actor.h"

namespace ioj {
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

}
