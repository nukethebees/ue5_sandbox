#pragma once

#include <ioj/math_types.h>

namespace ioj::presentation {
struct alignas(16) SparkParticleRecord {
    auto operator==(SparkParticleRecord const&) const -> bool = default;

    Vector<float, 4> initial_position_spawn_time{};
    Vector<float, 4> initial_velocity_lifetime{};
    Vector<float, 4> emissive_colour_size{};
    Vector<float, 4> streak_time_reserved{};
};

static_assert(sizeof(SparkParticleRecord) == 64);
}
