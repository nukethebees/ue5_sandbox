#pragma once

#include "ioj/sim/vector_types.h"

#include <array>
#include <cstdint>

namespace ioj::sim::collision {
struct EntityAABBs {
    using size_type = std::int32_t;

    static constexpr size_type num_rows{5};

    static constexpr size_type space_ship_index{0};
    static constexpr size_type static_turret_index{1};
    static constexpr size_type capital_ship_index{2};
    static constexpr size_type fighter_index{3};
    static constexpr size_type tube_spinner_index{4};

    static constexpr auto num() noexcept -> size_type { return num_rows; }

    constexpr void set_centre(size_type const index, Vector3f const centre) noexcept {
        auto const element{static_cast<std::size_t>(index)};
        centre_xs_[element] = centre.X;
        centre_ys_[element] = centre.Y;
        centre_zs_[element] = centre.Z;
    }

    constexpr void set_half_extents(size_type const index, Vector3f const half_extents) noexcept {
        auto const element{static_cast<std::size_t>(index)};
        half_extent_xs_[element] = half_extents.X;
        half_extent_ys_[element] = half_extents.Y;
        half_extent_zs_[element] = half_extents.Z;
    }

    [[nodiscard]] constexpr auto get_centre(size_type const index) const noexcept -> Vector3f {
        auto const element{static_cast<std::size_t>(index)};
        return ml::make_vector3f(centre_xs_[element], centre_ys_[element], centre_zs_[element]);
    }
    [[nodiscard]] constexpr auto get_half_extents(size_type const index) const noexcept
        -> Vector3f {
        auto const element{static_cast<std::size_t>(index)};
        return ml::make_vector3f(
            half_extent_xs_[element], half_extent_ys_[element], half_extent_zs_[element]);
    }
  private:
    std::array<float, num_rows> centre_xs_{};
    std::array<float, num_rows> centre_ys_{};
    std::array<float, num_rows> centre_zs_{};
    std::array<float, num_rows> half_extent_xs_{};
    std::array<float, num_rows> half_extent_ys_{};
    std::array<float, num_rows> half_extent_zs_{};
};
} // namespace ioj::sim::collision
