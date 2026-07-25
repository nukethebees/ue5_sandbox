#pragma once

#include "CoreMinimal.h"

namespace ml {
auto SANDBOXNATIVE_API get_random_point(FVector const& centre,
                                        float const min_dist,
                                        float const max_dist) -> FVector;
}
