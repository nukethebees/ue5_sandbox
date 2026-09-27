#pragma once

#include "sandbox/core/math_types.h"
#include "sandbox/core/sandbox_ismc_render.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace ml::sandbox_ismc {

inline constexpr float position_quantum{16.0f};
inline constexpr float quaternion_component_limit{0.7071067811865475244f};
inline constexpr float maximum_scale{31.875f};
// 10/10/10 smallest-three error is below 0.3 degrees. The 0.006 chord
// allowance also covers the normalized-input tolerance (squared length 1 +/- 1e-4).
inline constexpr float rotation_error_chord{0.006f};
inline constexpr float scale_error{0.0625f};

[[nodiscard]] inline auto is_normalized_quaternion(Quaternion4f quaternion) noexcept -> bool {
    auto const squared{HMM_DotQ(quaternion, quaternion)};
    return std::abs(squared - 1.0f) <= 1.0e-4f;
}

[[nodiscard]] inline auto quantize_quaternion_component(float value) noexcept -> std::uint32_t {
    auto const mapped{(value + quaternion_component_limit) *
                          (1023.0f / (2.0f * quaternion_component_limit)) +
                      0.5f};
    // Clamp before unsigned conversion, including tiny endpoint overshoots.
    return static_cast<std::uint32_t>(std::min(1023.0f, std::max(0.0f, mapped)));
}

// Smallest three: bits 0..1 select omitted XYZW, followed by three unsigned
// 10-bit components in XYZW order, mapped from [-1/sqrt(2), +1/sqrt(2)].
// Precondition: finite normalized input, validated before entering the packing loop.
[[nodiscard]] inline auto pack_normalized_quat32(Quaternion4f quaternion) noexcept -> Quat32 {
    auto const xy{std::abs(quaternion.Y) > std::abs(quaternion.X) ? 1U : 0U};
    auto const xyz{std::abs(quaternion.Z) > std::abs(quaternion.Elements[xy]) ? 2U : xy};
    auto const largest{std::abs(quaternion.W) > std::abs(quaternion.Elements[xyz]) ? 3U : xyz};
    auto const sign{quaternion.Elements[largest] < 0.0f ? -1.0f : 1.0f};
    auto const a{largest == 0U ? quaternion.Y : quaternion.X};
    auto const b{largest <= 1U ? quaternion.Z : quaternion.Y};
    auto const c{largest <= 2U ? quaternion.W : quaternion.Z};
    return Quat32{largest | (quantize_quaternion_component(a * sign) << 2U) |
                  (quantize_quaternion_component(b * sign) << 12U) |
                  (quantize_quaternion_component(c * sign) << 22U)};
}

// Reference/debug entry point for arbitrary nonzero quaternions, not the render hot path.
[[nodiscard]] inline auto pack_quat32(Quaternion4f quaternion) noexcept -> std::optional<Quat32> {
    auto const squared{HMM_DotQ(quaternion, quaternion)};
    if (!std::isfinite(squared) || squared < 1.0e-12f) {
        return std::nullopt;
    }
    return pack_normalized_quat32(HMM_MulQF(quaternion, 1.0f / std::sqrt(squared)));
}

[[nodiscard]] inline auto unpack_quat32(Quat32 packed) noexcept -> Quaternion4f {
    auto quaternion{HMM_Q(0.0f, 0.0f, 0.0f, 0.0f)};
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
        quaternion.Elements[index] = value;
        squared += value * value;
        shift += 10;
    }
    quaternion.Elements[largest] = std::sqrt(std::max(0.0f, 1.0f - squared));
    return quaternion;
}

[[nodiscard]] inline auto can_quantize_position(float position, float root) noexcept -> bool {
    auto const offset{(static_cast<double>(position) - root) * (1.0 / position_quantum)};
    return offset >= -32767.5 && offset < 32767.5;
}

// Precondition: can_quantize_position(position, root).
[[nodiscard]] inline auto quantize_position_unchecked(float position, float root) noexcept
    -> std::int16_t {
    // Round ties toward +infinity, independent of the process rounding mode.
    // Double subtraction is necessary at rounding boundaries: e.g. root=262144,
    // position=nextafter(8, 0) loses its side of the tie in float. Multiplication
    // and truncation avoid division/floor.
    auto const offset{(static_cast<double>(position) - root) * (1.0 / position_quantum)};
    auto const integral{static_cast<std::int32_t>(offset)};
    auto const fraction{offset - integral};
    return static_cast<std::int16_t>(integral + (fraction >= 0.5) - (fraction < -0.5));
}

// Checked entry point for domain setup and tests, not the render hot path.
[[nodiscard]] inline auto quantize_position(float position, float root) noexcept
    -> std::optional<std::int16_t> {
    if (!can_quantize_position(position, root)) {
        return std::nullopt;
    }
    return quantize_position_unchecked(position, root);
}

// Specialized float encoder for the existing generated unsigned Q5.3 storage type.
// Precondition: finite scale in [0, maximum_scale].
[[nodiscard]] inline auto pack_scale_unchecked(float scale) noexcept -> Scale8 {
    auto const scaled{scale * 8.0f};
    auto const integral{static_cast<std::uint32_t>(scaled)};
    auto const fraction{scaled - static_cast<float>(integral)};
    auto const round_up{fraction > 0.5f || (fraction == 0.5f && (integral & 1U) != 0)};
    return Scale8::from_raw(static_cast<std::uint8_t>(integral + round_up));
}

// Checked entry point for tests/debugging, not the render hot path.
[[nodiscard]] inline auto pack_scale(float scale) noexcept -> std::optional<Scale8> {
    if (!(scale >= 0.0f && scale <= maximum_scale)) {
        return std::nullopt;
    }
    return pack_scale_unchecked(scale);
}

// Radius is measured about the mesh origin, including an off-centre mesh AABB.
[[nodiscard]] constexpr auto geometry_error(float mesh_radius) noexcept -> float {
    return position_quantum * 0.5f +
           mesh_radius * (scale_error + rotation_error_chord * maximum_scale);
}

[[nodiscard]] inline auto position_root(Vector3f minimum, Vector3f maximum) noexcept
    -> std::optional<Vector3f> {
    Vector3f root{};
    for (auto axis{0U}; axis < 3; ++axis) {
        if (!std::isfinite(minimum.Elements[axis]) || !std::isfinite(maximum.Elements[axis]) ||
            minimum.Elements[axis] > maximum.Elements[axis]) {
            return std::nullopt;
        }
        auto const center{(static_cast<double>(minimum.Elements[axis]) + maximum.Elements[axis]) *
                          0.5};
        root.Elements[axis] =
            static_cast<float>(std::floor(center / position_quantum + 0.5) * position_quantum);
        if (!quantize_position(minimum.Elements[axis], root.Elements[axis]) ||
            !quantize_position(maximum.Elements[axis], root.Elements[axis])) {
            return std::nullopt;
        }
    }
    return root;
}

static_assert(std::endian::native == std::endian::little);
static_assert(std::is_standard_layout_v<PackedTransform>);
static_assert(std::is_trivially_copyable_v<PackedTransform>);
static_assert(sizeof(Scale8) == 1);
static_assert(sizeof(Quat32) == 4);
static_assert(sizeof(PackedTransform) == 16);
static_assert(alignof(PackedTransform) == 4);
static_assert(offsetof(PackedTransform, position) == 0);
static_assert(offsetof(PackedTransform, reserved_0) == 6);
static_assert(offsetof(PackedTransform, rotation) == 8);
static_assert(offsetof(PackedTransform, scale) == 12);
static_assert(offsetof(PackedTransform, reserved_1) == 15);

}
