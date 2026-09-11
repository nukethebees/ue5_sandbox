#include "noise_sampling.h"

#include "image_generation_utilities.h"

namespace sandbox::image::noise {
namespace {
constexpr float inverse_uint24{1.0f / 16777215.0f};

auto hash_lattice(std::int32_t const x, std::int32_t const y, std::uint32_t const seed)
    -> std::uint32_t {
    auto value{seed ^ (static_cast<std::uint32_t>(x) * 0x9E3779B9u) ^
               (static_cast<std::uint32_t>(y) * 0x85EBCA6Bu)};
    value ^= value >> 16u;
    value *= 0x7FEB352Du;
    value ^= value >> 15u;
    value *= 0x846CA68Bu;
    value ^= value >> 16u;
    return value;
}

auto value_noise(float const x, float const y, std::uint32_t const seed) -> float {
    auto const x0{generation::floor_to_int(x)};
    auto const y0{generation::floor_to_int(y)};
    auto const tx{generation::smooth_step(0.0f, 1.0f, x - static_cast<float>(x0))};
    auto const ty{generation::smooth_step(0.0f, 1.0f, y - static_cast<float>(y0))};
    auto const top{
        generation::lerp(lattice_value(x0, y0, seed), lattice_value(x0 + 1, y0, seed), tx)};
    auto const bottom{
        generation::lerp(lattice_value(x0, y0 + 1, seed), lattice_value(x0 + 1, y0 + 1, seed), tx)};
    return generation::lerp(top, bottom, ty);
}

auto periodic_lattice_value(std::int32_t const x,
                            std::int32_t const y,
                            std::int32_t const period_x,
                            std::int32_t const period_y,
                            std::uint32_t const seed) -> float {
    auto const wrapped_x{((x % period_x) + period_x) % period_x};
    auto const wrapped_y{((y % period_y) + period_y) % period_y};
    return lattice_value(wrapped_x, wrapped_y, seed);
}

auto periodic_value_noise(float const x,
                          float const y,
                          std::int32_t const period_x,
                          std::int32_t const period_y,
                          std::uint32_t const seed) -> float {
    auto const x0{generation::floor_to_int(x)};
    auto const y0{generation::floor_to_int(y)};
    auto const tx{generation::smooth_step(0.0f, 1.0f, x - static_cast<float>(x0))};
    auto const ty{generation::smooth_step(0.0f, 1.0f, y - static_cast<float>(y0))};
    auto const top{generation::lerp(periodic_lattice_value(x0, y0, period_x, period_y, seed),
                                    periodic_lattice_value(x0 + 1, y0, period_x, period_y, seed),
                                    tx)};
    auto const bottom{
        generation::lerp(periodic_lattice_value(x0, y0 + 1, period_x, period_y, seed),
                         periodic_lattice_value(x0 + 1, y0 + 1, period_x, period_y, seed),
                         tx)};
    return generation::lerp(top, bottom, ty);
}
}

DeterministicRandom::DeterministicRandom(std::uint32_t const seed)
    : state_{seed} {}

auto DeterministicRandom::next_unit() -> float {
    state_ = state_ * 1664525u + 1013904223u;
    return static_cast<float>(state_ >> 8u) * inverse_uint24;
}

auto lattice_value(std::int32_t const x, std::int32_t const y, std::uint32_t const seed) -> float {
    return static_cast<float>(hash_lattice(x, y, seed) & 0x00FFFFFFu) * inverse_uint24;
}

auto fractal_noise_sample(float const x,
                          float const y,
                          std::int32_t const width,
                          std::int32_t const height,
                          std::uint32_t const seed,
                          float const base_scale,
                          std::int32_t const octave_count,
                          float const persistence,
                          bool const tileable) -> float {
    auto const base_period_x{
        std::max(1, generation::round_to_int(static_cast<float>(width) / base_scale))};
    auto const base_period_y{
        std::max(1, generation::round_to_int(static_cast<float>(height) / base_scale))};
    float value{0.0f};
    float amplitude{1.0f};
    float total_amplitude{0.0f};
    float scale{base_scale};
    for (std::int32_t octave{0}; octave < octave_count; ++octave) {
        auto const octave_seed{seed + static_cast<std::uint32_t>(octave) * 0x9E3779B9u};
        if (tileable) {
            auto const octave_multiplier{1 << octave};
            auto const period_x{base_period_x * octave_multiplier};
            auto const period_y{base_period_y * octave_multiplier};
            auto const sample_x{width > 1 ? x / static_cast<float>(width - 1) * period_x : 0.0f};
            auto const sample_y{height > 1 ? y / static_cast<float>(height - 1) * period_y : 0.0f};
            value += periodic_value_noise(sample_x, sample_y, period_x, period_y, octave_seed) *
                     amplitude;
        } else {
            value += value_noise(x / scale, y / scale, octave_seed) * amplitude;
        }
        total_amplitude += amplitude;
        amplitude *= persistence;
        scale *= 0.5f;
    }
    return total_amplitude > 0.0f ? value / total_amplitude : 0.0f;
}

}
