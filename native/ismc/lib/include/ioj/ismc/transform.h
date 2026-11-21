#pragma once

#include "ioj/ismc/quat32_packing.h"
#include "ioj/ismc/render.h"
#include "ioj/math_types.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ioj::ismc {

// Radius is measured about the mesh origin, including an off-centre mesh AABB.
[[nodiscard]] constexpr auto geometry_error(float mesh_radius) noexcept -> float {
    return mesh_radius * rotation_error_chord;
}

static_assert(std::endian::native == std::endian::little);
static_assert(std::is_standard_layout_v<PackedTransform>);
static_assert(std::is_trivially_copyable_v<PackedTransform>);
static_assert(sizeof(PackedTransform) == 16);
static_assert(alignof(PackedTransform) == 4);
static_assert(offsetof(PackedTransform, position) == 0);
static_assert(offsetof(PackedTransform, rotation) == 12);

}
