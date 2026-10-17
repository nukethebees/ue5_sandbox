#pragma once

#include "ioj/enum_array.h"
#include "ioj/sim/entities/entity_type.h"
#include "ioj/vector_types.h"

#include <cstddef>

namespace ioj::sim::collision {
struct EntityAABBs {
    constexpr void set_centre(EntityType const type, Vector3f const centre) noexcept {
        centre_xs_[type] = centre.x();
        centre_ys_[type] = centre.y();
        centre_zs_[type] = centre.z();
    }

    constexpr void set_half_extents(EntityType const type, Vector3f const half_extents) noexcept {
        half_extent_xs_[type] = half_extents.x();
        half_extent_ys_[type] = half_extents.y();
        half_extent_zs_[type] = half_extents.z();
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
    using Values = ml::EnumArray<EntityType, float>;

    Values centre_xs_{};
    Values centre_ys_{};
    Values centre_zs_{};
    Values half_extent_xs_{};
    Values half_extent_ys_{};
    Values half_extent_zs_{};
};
} // namespace ioj::sim::collision
