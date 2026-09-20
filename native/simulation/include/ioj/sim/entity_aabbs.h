#pragma once

#include "ioj/sim/entity_type.h"
#include "ioj/sim/vector_types.h"
#include "sandbox/core/enum_array.h"

#include <cstddef>

namespace ioj::sim::collision {
struct EntityAABBs {
    constexpr void set_centre(EntityType const type, Vector3f const centre) noexcept {
        centre_xs_[type] = centre.X;
        centre_ys_[type] = centre.Y;
        centre_zs_[type] = centre.Z;
    }

    constexpr void set_half_extents(EntityType const type, Vector3f const half_extents) noexcept {
        half_extent_xs_[type] = half_extents.X;
        half_extent_ys_[type] = half_extents.Y;
        half_extent_zs_[type] = half_extents.Z;
    }

    [[nodiscard]] constexpr auto get_centre(EntityType const type) const noexcept -> Vector3f {
        return ml::make_vector3f(centre_xs_[type], centre_ys_[type], centre_zs_[type]);
    }
    [[nodiscard]] constexpr auto get_half_extents(EntityType const type) const noexcept
        -> Vector3f {
        return ml::make_vector3f(
            half_extent_xs_[type], half_extent_ys_[type], half_extent_zs_[type]);
    }
  private:
    using Values =
        ml::EnumArray<EntityType, float, static_cast<std::size_t>(EntityType::COUNT)>;

    Values centre_xs_{};
    Values centre_ys_{};
    Values centre_zs_{};
    Values half_extent_xs_{};
    Values half_extent_ys_{};
    Values half_extent_zs_{};
};
} // namespace ioj::sim::collision
