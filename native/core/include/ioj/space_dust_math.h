#pragma once

#include <ioj/math_types.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ioj::space_dust {
inline constexpr std::int32_t maximum_particle_count{2048};

struct Tuning {
    bool enabled{true};
    std::int32_t particle_count{96};
    std::uint32_t random_seed{1337};
    Vector3f volume_dimensions{ioj::Vector3f{16000.0f, 12000.0f, 8000.0f}};
    float particle_size{8.0f};
    float brightness{0.35f};
    Vector3f colour{ioj::Vector3f{0.82f, 0.9f, 1.0f}};
    float minimum_visible_speed{100.0f};
    float full_visible_speed{2000.0f};
    float streak_seconds{0.0125f};
    float minimum_motion_pixels{0.75f};
    float full_motion_pixels{4.0f};
    float maximum_streak_pixels{24.0f};
    float volume_edge_fade_fraction{0.15f};
};

using WorldLocation = ioj::Vector<double, 3>;

struct ParticleVariation {
    float size_scale{};
    float intensity_scale{};
};

[[nodiscard]] inline auto normalise_tuning(Tuning tuning) noexcept -> Tuning {
    tuning.particle_count = std::clamp(tuning.particle_count, 0, maximum_particle_count);
    tuning.volume_dimensions.x() = std::max(tuning.volume_dimensions.x(), 1.0f);
    tuning.volume_dimensions.y() = std::max(tuning.volume_dimensions.y(), 1.0f);
    tuning.volume_dimensions.z() = std::max(tuning.volume_dimensions.z(), 1.0f);
    tuning.particle_size = std::max(tuning.particle_size, 0.0f);
    tuning.brightness = std::max(tuning.brightness, 0.0f);
    tuning.colour.x() = std::max(tuning.colour.x(), 0.0f);
    tuning.colour.y() = std::max(tuning.colour.y(), 0.0f);
    tuning.colour.z() = std::max(tuning.colour.z(), 0.0f);
    tuning.minimum_visible_speed = std::max(tuning.minimum_visible_speed, 0.0f);
    tuning.full_visible_speed =
        std::max(tuning.full_visible_speed,
                 tuning.minimum_visible_speed + std::numeric_limits<float>::epsilon());
    tuning.streak_seconds = std::max(tuning.streak_seconds, 0.0f);
    tuning.minimum_motion_pixels = std::max(tuning.minimum_motion_pixels, 0.0f);
    tuning.full_motion_pixels = std::max(
        tuning.full_motion_pixels,
        std::nextafter(tuning.minimum_motion_pixels, std::numeric_limits<float>::infinity()));
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
    return ioj::Vector3f{positive_modulo(world_location.x(), volume_dimensions.x()),
                         positive_modulo(world_location.y(), volume_dimensions.y()),
                         positive_modulo(world_location.z(), volume_dimensions.z())};
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
    return ioj::Vector3f{
        random_unit(seed), random_unit(seed ^ 0x68bc21ebu), random_unit(seed ^ 0x02e5be93u)};
}

[[nodiscard]] constexpr auto make_particle_variation(std::uint32_t const instance_id,
                                                     std::uint32_t const random_seed) noexcept
    -> ParticleVariation {
    auto const seed{instance_id ^ random_seed};
    return {
        .size_scale = 0.8f + random_unit(seed ^ 0xa511e9b3u) * 0.4f,
        .intensity_scale = 0.65f + random_unit(seed ^ 0x63d83595u) * 0.35f,
    };
}
}
