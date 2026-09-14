#pragma once

#include "ioj/sim/rotator_types.h"
#include "ioj/sim/vector_types.h"

#include <cstdint>

namespace ioj::sim::fighters {
struct FirePointCandidate {
    Vector3f location;
    Vector3f trace_start;
    Vector3f trace_end;
};

inline constexpr std::uint32_t fire_point_candidate_count{16};

[[nodiscard]] auto make_fire_point_candidate(Vector3f target_location,
                                             Vector3f reference_location,
                                             float fire_point_distance,
                                             float trace_end_offset,
                                             float desired_attack_distance,
                                             std::uint32_t integral_bias,
                                             float float_bias,
                                             std::uint32_t candidate_order) noexcept
    -> FirePointCandidate;

[[nodiscard]] auto fire_point_rotation(Rotator3f base_rotation,
                                       std::uint32_t integral_bias,
                                       float float_bias,
                                       std::uint32_t candidate_order) noexcept -> Rotator3f;
}
