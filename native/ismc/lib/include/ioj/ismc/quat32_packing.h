#pragma once

#include "ioj/ismc/quat32.h"
#include "ioj/quaternion.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace ioj::ismc {

inline constexpr float quaternion_component_limit{0.7071067811865475244f};
// 10/10/10 smallest-three error is below 0.3 degrees. The 0.006 chord
// allowance also covers the normalized-input tolerance (squared length 1 +/- 1e-4).
inline constexpr float rotation_error_chord{0.006f};

[[nodiscard]] inline auto is_normalized_quaternion(Quaternion4f quaternion) noexcept -> bool {
    auto const squared{dot(quaternion, quaternion)};
    return std::abs(squared - 1.0f) <= 1.0e-4f;
}

[[nodiscard]] inline auto quantize_quaternion_component(float value) noexcept -> std::uint32_t {
    constexpr float quantization_scale{1023.0f / (2.0f * quaternion_component_limit)};
    auto const mapped{(value + quaternion_component_limit) * quantization_scale + 0.5f};
    // Clamp before unsigned conversion, including tiny endpoint overshoots.
    return static_cast<std::uint32_t>(std::min(1023.0f, std::max(0.0f, mapped)));
}

// Smallest three: bits 0..1 select omitted XYZW, followed by three unsigned
// 10-bit components in XYZW order, mapped from [-1/sqrt(2), +1/sqrt(2)].
// Precondition: finite normalized input, validated before entering the packing loop.
[[nodiscard]] inline auto pack_normalized_quat32(float x, float y, float z, float w) noexcept
    -> Quat32 {
    auto const xy{std::abs(y) > std::abs(x) ? 1U : 0U};
    auto const xy_value{xy == 0U ? x : y};
    auto const xyz{std::abs(z) > std::abs(xy_value) ? 2U : xy};
    auto const xyz_value{xyz == 2U ? z : xy_value};
    auto const largest{std::abs(w) > std::abs(xyz_value) ? 3U : xyz};
    auto const largest_value{largest == 3U ? w : xyz_value};
    auto const sign{largest_value < 0.0f ? -1.0f : 1.0f};
    auto const a{largest == 0U ? y : x};
    auto const b{largest <= 1U ? z : y};
    auto const c{largest <= 2U ? w : z};
    return Quat32{largest | (quantize_quaternion_component(a * sign) << 2U) |
                  (quantize_quaternion_component(b * sign) << 12U) |
                  (quantize_quaternion_component(c * sign) << 22U)};
}

[[nodiscard]] inline auto pack_normalized_quat32(Quaternion4f quaternion) noexcept -> Quat32 {
    return pack_normalized_quat32(quaternion.x(), quaternion.y(), quaternion.z(), quaternion.w());
}

// Reference/debug entry point for arbitrary nonzero quaternions, not the render hot path.
[[nodiscard]] inline auto pack_quat32(Quaternion4f quaternion) noexcept -> std::optional<Quat32> {
    auto const squared{dot(quaternion, quaternion)};
    if (!std::isfinite(squared) || squared < 1.0e-12f) {
        return std::nullopt;
    }
    return pack_normalized_quat32(quaternion * (1.0f / std::sqrt(squared)));
}

[[nodiscard]] inline auto unpack_quat32(Quat32 packed) noexcept -> Quaternion4f {
    auto quaternion{Quaternion4f{0.0f, 0.0f, 0.0f, 0.0f}};
    auto const largest{packed.bits & 3U};
    auto shift{2U};
    auto squared{0.0f};
    for (auto index{0U}; index < 4; ++index) {
        if (index == largest) {
            continue;
        }
        auto const code{(packed.bits >> shift) & 1023U};
        auto const value{static_cast<float>(code) * (2.0f * quaternion_component_limit / 1023.0f) -
                         quaternion_component_limit};
        quaternion[index] = value;
        squared += value * value;
        shift += 10;
    }
    quaternion[largest] = std::sqrt(std::max(0.0f, 1.0f - squared));
    return quaternion;
}

static_assert(sizeof(Quat32) == 4);

}
