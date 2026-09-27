#pragma once

#include "sandbox/core/math_types.h"
#include "sandbox/core/sandbox_ismc_scale.h"

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

// Smallest three: bits 0..1 select omitted XYZW, followed by three unsigned
// 10-bit components in XYZW order, mapped from [-1/sqrt(2), +1/sqrt(2)].
struct Quat32 {
    std::uint32_t bits{};
};

[[nodiscard]] inline auto pack_quat32(Quaternion4f quaternion) noexcept -> std::optional<Quat32> {
    auto const squared{HMM_DotQ(quaternion, quaternion)};
    if (!std::isfinite(squared) || squared < 1.0e-12f) {
        return std::nullopt;
    }
    quaternion = HMM_MulQF(quaternion, 1.0f / std::sqrt(squared));
    auto largest{0U};
    for (auto index{1U}; index < 4; ++index) {
        if (std::abs(quaternion.Elements[index]) > std::abs(quaternion.Elements[largest])) {
            largest = index;
        }
    }
    auto const sign{quaternion.Elements[largest] < 0.0f ? -1.0f : 1.0f};
    auto bits{largest};
    auto shift{2U};
    for (auto index{0U}; index < 4; ++index) {
        if (index == largest) {
            continue;
        }
        auto const value{quaternion.Elements[index] * sign};
        auto const code{static_cast<std::uint32_t>(std::floor(
            (value + quaternion_component_limit) * (1023.0f / (2.0f * quaternion_component_limit)) +
            0.5f))};
        bits |= std::min(code, 1023U) << shift;
        shift += 10;
    }
    return Quat32{bits};
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

[[nodiscard]] inline auto quantize_position(float position, float root) noexcept
    -> std::optional<std::int16_t> {
    // Round ties toward +infinity, independent of the process rounding mode.
    auto const offset{std::floor((static_cast<double>(position) - root) / position_quantum + 0.5)};
    if (!std::isfinite(offset) || offset < -32767.0 || offset > 32767.0) {
        return std::nullopt;
    }
    return static_cast<std::int16_t>(offset);
}

struct PackedTransform {
    std::int16_t position[3]{};
    std::uint16_t reserved_0{};
    Quat32 rotation{};
    Scale8 scale[3]{};
    std::uint8_t reserved_1{};
};

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
