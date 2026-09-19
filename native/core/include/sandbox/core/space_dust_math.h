#pragma once

#include <sandbox/core/math_types.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ml::space_dust {
inline constexpr std::int32_t maximum_particle_count{65536};

struct Tuning {
    bool enabled{true};
    std::int32_t particle_count{1024};
    std::uint32_t random_seed{1337};
    Vector3f volume_dimensions{make_vector3f(16000.0f, 12000.0f, 8000.0f)};
    float particle_size{8.0f};
    float brightness{0.35f};
    Vector3f colour{make_vector3f(0.82f, 0.9f, 1.0f)};
    float minimum_visible_speed{1000.0f};
    float full_visible_speed{8000.0f};
    float streak_seconds{0.0125f};
    float maximum_streak_pixels{24.0f};
    float volume_edge_fade_fraction{0.15f};
};

struct WorldLocation {
    double x{};
    double y{};
    double z{};
};

[[nodiscard]] inline auto normalise_tuning(Tuning tuning) noexcept -> Tuning {
    tuning.particle_count = std::clamp(tuning.particle_count, 0, maximum_particle_count);
    tuning.volume_dimensions.X = std::max(tuning.volume_dimensions.X, 1.0f);
    tuning.volume_dimensions.Y = std::max(tuning.volume_dimensions.Y, 1.0f);
    tuning.volume_dimensions.Z = std::max(tuning.volume_dimensions.Z, 1.0f);
    tuning.particle_size = std::max(tuning.particle_size, 0.0f);
    tuning.brightness = std::max(tuning.brightness, 0.0f);
    tuning.colour.X = std::max(tuning.colour.X, 0.0f);
    tuning.colour.Y = std::max(tuning.colour.Y, 0.0f);
    tuning.colour.Z = std::max(tuning.colour.Z, 0.0f);
    tuning.minimum_visible_speed = std::max(tuning.minimum_visible_speed, 0.0f);
    tuning.full_visible_speed =
        std::max(tuning.full_visible_speed,
                 tuning.minimum_visible_speed + std::numeric_limits<float>::epsilon());
    tuning.streak_seconds = std::max(tuning.streak_seconds, 0.0f);
    tuning.maximum_streak_pixels = std::max(tuning.maximum_streak_pixels, 0.0f);
    tuning.volume_edge_fade_fraction = std::clamp(tuning.volume_edge_fade_fraction, 0.0f, 0.49f);
    return tuning;
}

[[nodiscard]] inline auto positive_modulo(double const value, double const divisor) noexcept
    -> float {
    auto remainder{std::fmod(value, divisor)};
    if (remainder < 0.0) {
        remainder += divisor;
    }
    return static_cast<float>(remainder);
}

[[nodiscard]] inline auto make_translation_phase(WorldLocation const world_location,
                                                 Vector3f const volume_dimensions) noexcept
    -> Vector3f {
    return make_vector3f(positive_modulo(world_location.x, volume_dimensions.X),
                         positive_modulo(world_location.y, volume_dimensions.Y),
                         positive_modulo(world_location.z, volume_dimensions.Z));
}

[[nodiscard]] constexpr auto hash(std::uint32_t value) noexcept -> std::uint32_t {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

[[nodiscard]] constexpr auto random_unit(std::uint32_t const value) noexcept -> float {
    return static_cast<float>(hash(value) & 0x00ffffffu) / 16777216.0f;
}

[[nodiscard]] constexpr auto make_random_position(std::uint32_t const instance_id,
                                                  std::uint32_t const random_seed) noexcept
    -> Vector3f {
    auto const seed{instance_id ^ random_seed};
    return make_vector3f(
        random_unit(seed), random_unit(seed ^ 0x68bc21ebu), random_unit(seed ^ 0x02e5be93u));
}
}
