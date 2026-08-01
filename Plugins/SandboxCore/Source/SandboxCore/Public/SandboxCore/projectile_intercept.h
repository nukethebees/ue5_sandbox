#pragma once

#include "soa_vectors.h"

#include "Containers/ArrayView.h"
#include "Math/Vector.h"

namespace ml {
auto SANDBOXCORE_API solve_intercept_time(FVector3f const& shooter_pos,
                                          FVector3f const& target_pos,
                                          FVector3f const& target_vel,
                                          float const projectile_speed) -> float;
}

namespace ml::detail::solve_intercept_times_loop {
void SANDBOXCORE_API solve_intercept_times(TArrayView<float> out_intercept_times,
                                           FVectors3f::ConstView shooter_positions,
                                           FVectors3f::ConstView target_positions,
                                           FVectors3f::ConstView target_velocities,
                                           float projectile_speed);
}
namespace ml::detail::solve_intercept_times_aos {
void SANDBOXCORE_API solve_intercept_times(TArrayView<float> out_intercept_times,
                                           FVectors3f::ConstView shooter_positions,
                                           FVectors3f::ConstView target_positions,
                                           FVectors3f::ConstView target_velocities,
                                           float projectile_speed);
}

namespace ml {
using detail::solve_intercept_times_loop::solve_intercept_times;
}
