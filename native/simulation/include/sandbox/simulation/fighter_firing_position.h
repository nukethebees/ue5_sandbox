#pragma once

#include "sandbox/simulation/rotator_types.h"
#include "sandbox/simulation/vector_types.h"

#include <cstdint>

namespace ml::simulation::fighters {
struct FirePointCandidate {
    Vector3f location;
    Vector3f trace_start;
    Vector3f trace_end;
};

inline constexpr std::uint32_t fire_point_candidate_count{16};

[[nodiscard]] auto fire_point_rotation(Rotator3f base_rotation,
                                       std::uint32_t integral_bias,
                                       float float_bias,
                                       std::uint32_t candidate_order) noexcept -> Rotator3f;
}
