#include "SandboxNative/spatial.h"

#include <sandbox/simulation/spatial.h>

namespace ml {
auto get_random_point(FVector const& centre, float const min_dist, float const max_dist)
    -> FVector {
    auto const direction{FMath::VRand()};
    auto const point{
        ml::simulation::sample_spherical_shell_point({centre.X, centre.Y, centre.Z},
                                                     {direction.X, direction.Y, direction.Z},
                                                     min_dist,
                                                     max_dist,
                                                     FMath::FRand())};
    return {point.x, point.y, point.z};
}
}
