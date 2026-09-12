#include <sandbox/core/rotation.h>

namespace ml::kernel {
template void rotate_towards_1d_degrees<float>(
    float const*, float const*, float, float, float*, std::int32_t) noexcept;
template void rotate_towards_1d_radians<float>(
    float const*, float const*, float, float, float*, std::int32_t) noexcept;
template void rotate_towards_1d_degrees_normalised<float>(
    float const*, float const*, float, float, float*, std::int32_t) noexcept;
template void rotate_towards_1d_radians_normalised<float>(
    float const*, float const*, float, float, float*, std::int32_t) noexcept;
template void rotate_towards_1d_degrees_normalised_in_place<float>(
    float*, float const*, float, float, std::int32_t) noexcept;
template void rotate_towards_1d_radians_normalised_in_place<float>(
    float*, float const*, float, float, std::int32_t) noexcept;
template void compute_desired_yaws_radians<float>(
    float const*, float const*, float const*, float const*, float*, std::int32_t) noexcept;
}
