#pragma once

#include "sandbox/simulation/vector_types.h"

#include <array>
#include <cstdint>

namespace ml::simulation::collision {
struct EntityAABBs {
    using size_type = std::int32_t;

    static constexpr size_type num_rows{5};

    static constexpr size_type space_ship_index{0};
    static constexpr size_type static_turret_index{1};
    static constexpr size_type capital_ship_index{2};
    static constexpr size_type fighter_index{3};
    static constexpr size_type tube_spinner_index{4};

    static constexpr auto num() noexcept -> size_type { return num_rows; }

    [[nodiscard]] constexpr auto get_centre(size_type const index) const noexcept -> Vector3f {
        auto const element{static_cast<std::size_t>(index)};
        return make_vector3f(centre_xs[element], centre_ys[element], centre_zs[element]);
    }
    [[nodiscard]] constexpr auto get_half_extents(size_type const index) const noexcept
        -> Vector3f {
        auto const element{static_cast<std::size_t>(index)};
        return make_vector3f(
            half_extent_xs[element], half_extent_ys[element], half_extent_zs[element]);
    }

    std::array<float, num_rows> centre_xs{};
    std::array<float, num_rows> centre_ys{};
    std::array<float, num_rows> centre_zs{};
    std::array<float, num_rows> half_extent_xs{};
    std::array<float, num_rows> half_extent_ys{};
    std::array<float, num_rows> half_extent_zs{};
};
} // namespace ml::simulation::collision
