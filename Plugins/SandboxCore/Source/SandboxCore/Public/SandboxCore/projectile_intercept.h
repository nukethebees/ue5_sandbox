#pragma once

#include "CoreMinimal.h"

namespace ml {
auto SANDBOXCORE_API solve_intercept_time(FVector3f const& shooter_pos,
                                          FVector3f const& target_pos,
                                          FVector3f const& target_vel,
                                          float const projectile_speed) -> float;
}
