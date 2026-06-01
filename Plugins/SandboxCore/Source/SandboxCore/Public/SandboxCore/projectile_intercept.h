#pragma once

#include "CoreMinimal.h"

namespace ml {
auto SANDBOXCORE_API solve_intercept_time(FVector const& shooter_pos,
                                          FVector const& target_pos,
                                          FVector const& target_vel,
                                          float const projectile_speed) -> float;
}
