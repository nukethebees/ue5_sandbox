#include "spatial.h"

namespace ml {
auto get_random_point(FVector const& centre, float const min_dist, float const max_dist)
    -> FVector {
    auto const dir{FMath::VRand()};

    auto const dist{FMath::Sqrt(FMath::FRandRange(min_dist * min_dist, max_dist * max_dist))};

    return centre + dir * dist;
}
}
