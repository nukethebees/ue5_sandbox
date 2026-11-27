#pragma once

#include <ioj/math_types.h>
#include <ioj/sim/lasers/laser_source.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::presentation {
namespace vectors3f {
struct ConstView {
    auto operator[](std::size_t index) const -> Vector3f {
        return {xs[index], ys[index], zs[index]};
    }
    auto num() const noexcept -> std::uint32_t { return static_cast<std::uint32_t>(xs.size()); }

    std::span<float const> xs;
    std::span<float const> ys;
    std::span<float const> zs;
};
}
namespace rotators3f {
struct ConstView {
    std::span<float const> pitches;
    std::span<float const> yaws;
    std::span<float const> rolls;
};
}
namespace world_aabbs {
struct ConstView {
    vectors3f::ConstView min_points;
    vectors3f::ConstView max_points;
};
}
namespace laser_hit_details {
struct ConstView {
    vectors3f::ConstView locations;
    vectors3f::ConstView emission_directions;
    std::span<sim::LaserSource const> sources;
};
}
}
