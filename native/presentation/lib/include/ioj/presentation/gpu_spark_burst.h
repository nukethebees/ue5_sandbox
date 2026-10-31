#pragma once

#include <ioj/math_types.h>

#include <cstdint>
#include <limits>

namespace ioj::presentation {
struct alignas(16) GpuSparkBurst {
    inline static constexpr std::uint32_t direct_particle_indices{
        std::numeric_limits<std::uint32_t>::max()};

    Vector<float, 4> location_spawn_time{};
    Vector<float, 4> direction_cone_cosine{};
    Vector<float, 4> colour_intensity{};
    Vector<float, 4> speed_lifetime_ranges{};
    Vector<float, 4> size_streak_reserved{};
    Vector<std::uint32_t, 4> allocation{};
};

static_assert(sizeof(GpuSparkBurst) == 96);
}
