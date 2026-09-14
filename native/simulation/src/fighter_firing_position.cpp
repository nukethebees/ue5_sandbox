#include "ioj/sim/fighter_firing_position.h"
#include "ioj/sim/rotator_math.h"
#include "sandbox/core/vector_normalization.h"

#include <array>

namespace ioj::sim::fighters {
auto make_fire_point_candidate(Vector3f const target_location,
                               Vector3f const reference_location,
                               float const fire_point_distance,
                               float const trace_end_offset,
                               float const desired_attack_distance,
                               std::uint32_t const integral_bias,
                               float const float_bias,
                               std::uint32_t const candidate_order) noexcept -> FirePointCandidate {
    auto const base_direction{ml::native_math::safe_normal(reference_location - target_location)};
    auto const rotation{fire_point_rotation(
        direction_to_rotation(base_direction), integral_bias, float_bias, candidate_order)};
    auto const candidate_location{target_location +
                                  forward_direction(rotation) * desired_attack_distance};

    std::array<double, 3> const target{target_location.X, target_location.Y, target_location.Z};
    std::array<double, 3> const candidate{
        candidate_location.X, candidate_location.Y, candidate_location.Z};
    std::array<double, 3> aim{
        target[0] - candidate[0], target[1] - candidate[1], target[2] - candidate[2]};
    ml::native_math::safe_normal_components(aim[0], aim[1], aim[2]);
    std::array<double, 3> const start{candidate[0] + aim[0] * fire_point_distance,
                                      candidate[1] + aim[1] * fire_point_distance,
                                      candidate[2] + aim[2] * fire_point_distance};
    std::array<double, 3> direction{
        target[0] - start[0], target[1] - start[1], target[2] - start[2]};
    ml::native_math::safe_normal_components(direction[0], direction[1], direction[2]);
    return {candidate_location,
            HMM_V3(static_cast<float>(start[0]),
                   static_cast<float>(start[1]),
                   static_cast<float>(start[2])),
            HMM_V3(static_cast<float>(target[0] - direction[0] * trace_end_offset),
                   static_cast<float>(target[1] - direction[1] * trace_end_offset),
                   static_cast<float>(target[2] - direction[2] * trace_end_offset))};
}

namespace firing_position_detail {
struct FirePointAngleOffset {
    float yaw;
    float pitch;
};

inline constexpr std::array<FirePointAngleOffset, fire_point_candidate_count> angle_offsets{{
    {0.f, 0.f},
    {45.f, 0.f},
    {-45.f, 0.f},
    {90.f, 0.f},
    {-90.f, 0.f},
    {135.f, 0.f},
    {-135.f, 0.f},
    {180.f, 0.f},
    {0.f, 35.f},
    {90.f, 35.f},
    {180.f, 35.f},
    {-90.f, 35.f},
    {45.f, -35.f},
    {135.f, -35.f},
    {-135.f, -35.f},
    {-45.f, -35.f},
}};
}

auto fire_point_rotation(Rotator3f base_rotation,
                         std::uint32_t const integral_bias,
                         float const float_bias,
                         std::uint32_t const candidate_order) noexcept -> Rotator3f {
    auto const first_index{integral_bias % fire_point_candidate_count};
    auto const index{(first_index + candidate_order) % fire_point_candidate_count};
    auto const offset{firing_position_detail::angle_offsets[index]};
    base_rotation.yaw += float_bias * 360.f + offset.yaw;
    base_rotation.pitch += offset.pitch;
    return base_rotation;
}
}
