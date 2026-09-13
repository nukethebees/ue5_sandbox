#include "sandbox/simulation/fighter_firing_position.h"

#include <array>

namespace ml::simulation::fighters {
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
