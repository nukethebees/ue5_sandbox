#pragma once

#include "ioj/ismc/quat32_packing.h"
#include "ioj/ismc/render.h"
#include "ioj/math_types.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace ioj::ismc {

inline constexpr float position_quantum{16.0f};
inline constexpr float inverse_position_quantum{1.0f / position_quantum};
inline constexpr float position_quantization_half_step{0.5f};
inline constexpr float position_quantization_bound{
    static_cast<float>(std::numeric_limits<std::int16_t>::max()) + position_quantization_half_step};

[[nodiscard]] inline auto can_quantize_position(float position, float root) noexcept -> bool {
    auto const offset{(position - root) * inverse_position_quantum};
    return offset >= -position_quantization_bound && offset < position_quantization_bound;
}

// Precondition: can_quantize_position(position, root).
[[nodiscard]] inline auto quantize_position_unchecked(float position, float root) noexcept
    -> std::int16_t {
    // Round ties toward +infinity, independent of the process rounding mode.
    // Float subtraction may move a value essentially on a boundary by one bucket.
    auto const offset{(position - root) * inverse_position_quantum};
    auto const integral{static_cast<std::int32_t>(offset)};
    auto const fraction{offset - static_cast<float>(integral)};
    return static_cast<std::int16_t>(integral + (fraction >= position_quantization_half_step) -
                                     (fraction < -position_quantization_half_step));
}

// Checked entry point for domain setup and tests, not the render hot path.
[[nodiscard]] inline auto quantize_position(float position, float root) noexcept
    -> std::optional<std::int16_t> {
    if (!can_quantize_position(position, root)) {
        return std::nullopt;
    }
    return quantize_position_unchecked(position, root);
}

// Radius is measured about the mesh origin, including an off-centre mesh AABB.
[[nodiscard]] constexpr auto geometry_error(float mesh_radius) noexcept -> float {
    return position_quantum * position_quantization_half_step + mesh_radius * rotation_error_chord;
}

[[nodiscard]] inline auto position_root(Vector3f minimum, Vector3f maximum) noexcept
    -> std::optional<Vector3f> {
    Vector3f root{};
    for (auto axis{0U}; axis < 3; ++axis) {
        if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) ||
            minimum[axis] > maximum[axis]) {
            return std::nullopt;
        }
        auto const center{(static_cast<double>(minimum[axis]) + maximum[axis]) * 0.5};
        root[axis] = static_cast<float>(
            std::floor(center / position_quantum + position_quantization_half_step) *
            position_quantum);
        if (!quantize_position(minimum[axis], root[axis]) ||
            !quantize_position(maximum[axis], root[axis])) {
            return std::nullopt;
        }
    }
    return root;
}

static_assert(std::endian::native == std::endian::little);
static_assert(std::is_standard_layout_v<PackedTransform>);
static_assert(std::is_trivially_copyable_v<PackedTransform>);
static_assert(sizeof(PackedTransform) == 12);
static_assert(alignof(PackedTransform) == 4);
static_assert(offsetof(PackedTransform, position) == 0);
static_assert(offsetof(PackedTransform, reserved) == 6);
static_assert(offsetof(PackedTransform, rotation) == 8);

}
