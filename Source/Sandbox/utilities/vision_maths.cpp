#include "Sandbox/utilities/vision_maths.h"

#include "GameFramework/Actor.h"

#include "Sandbox/utilities/geometry.h"

namespace ml {
auto get_angle_from_fwd_vector(FVector standing_pt, FVector fwd, AActor const& target) -> double {
    auto const to_target{target.GetActorLocation() - standing_pt};
    auto const to_target_norm{to_target.GetSafeNormal()};
    auto const angle_between{ml::get_angle_between_norm_lines(fwd, to_target_norm)};

    return angle_between;
}
}
