#include "Generation/NoiseSampling.h"

#include "Generation/ImageGenerationUtilities.h"

namespace SandboxImages::GenLab::NoiseSampling {
namespace {
constexpr float inverse_uint24{1.0f / 16777215.0f};

auto hash_lattice(int32 const x, int32 const y, uint32 const seed) -> uint32 {
    auto value{seed ^ (static_cast<uint32>(x) * 0x9E3779B9u) ^
               (static_cast<uint32>(y) * 0x85EBCA6Bu)};
    value ^= value >> 16u;
    value *= 0x7FEB352Du;
    value ^= value >> 15u;
    value *= 0x846CA68Bu;
    value ^= value >> 16u;
    return value;
}

auto value_noise(float const x, float const y, uint32 const seed) -> float {
    auto const x0{FMath::FloorToInt(x)};
    auto const y0{FMath::FloorToInt(y)};
    auto const tx{ImageGeneration::smooth_step(0.0f, 1.0f, x - static_cast<float>(x0))};
    auto const ty{ImageGeneration::smooth_step(0.0f, 1.0f, y - static_cast<float>(y0))};
    auto const top{FMath::Lerp(lattice_value(x0, y0, seed), lattice_value(x0 + 1, y0, seed), tx)};
    auto const bottom{
        FMath::Lerp(lattice_value(x0, y0 + 1, seed), lattice_value(x0 + 1, y0 + 1, seed), tx)};
    return FMath::Lerp(top, bottom, ty);
}

auto periodic_lattice_value(int32 const x,
                            int32 const y,
                            int32 const period_x,
                            int32 const period_y,
                            uint32 const seed) -> float {
    auto const wrapped_x{((x % period_x) + period_x) % period_x};
    auto const wrapped_y{((y % period_y) + period_y) % period_y};
    return lattice_value(wrapped_x, wrapped_y, seed);
}

auto periodic_value_noise(float const x,
                          float const y,
                          int32 const period_x,
                          int32 const period_y,
                          uint32 const seed) -> float {
    auto const x0{FMath::FloorToInt(x)};
    auto const y0{FMath::FloorToInt(y)};
    auto const tx{ImageGeneration::smooth_step(0.0f, 1.0f, x - static_cast<float>(x0))};
    auto const ty{ImageGeneration::smooth_step(0.0f, 1.0f, y - static_cast<float>(y0))};
    auto const top{FMath::Lerp(periodic_lattice_value(x0, y0, period_x, period_y, seed),
                               periodic_lattice_value(x0 + 1, y0, period_x, period_y, seed),
                               tx)};
    auto const bottom{FMath::Lerp(periodic_lattice_value(x0, y0 + 1, period_x, period_y, seed),
                                  periodic_lattice_value(x0 + 1, y0 + 1, period_x, period_y, seed),
                                  tx)};
    return FMath::Lerp(top, bottom, ty);
}
}

FDeterministicRandom::FDeterministicRandom(uint32 const seed)
    : state_{seed} {}

auto FDeterministicRandom::next_unit() -> float {
    state_ = state_ * 1664525u + 1013904223u;
    return static_cast<float>(state_ >> 8u) * inverse_uint24;
}

auto lattice_value(int32 const x, int32 const y, uint32 const seed) -> float {
    return static_cast<float>(hash_lattice(x, y, seed) & 0x00FFFFFFu) * inverse_uint24;
}

auto fractal_noise_sample(float const x,
                          float const y,
                          int32 const width,
                          int32 const height,
                          uint32 const seed,
                          float const base_scale,
                          int32 const octave_count,
                          float const persistence,
                          bool const tileable) -> float {
    auto const base_period_x{
        FMath::Max(1, FMath::RoundToInt(static_cast<float>(width) / base_scale))};
    auto const base_period_y{
        FMath::Max(1, FMath::RoundToInt(static_cast<float>(height) / base_scale))};
    float value{0.0f};
    float amplitude{1.0f};
    float total_amplitude{0.0f};
    float scale{base_scale};
    for (int32 octave{0}; octave < octave_count; ++octave) {
        auto const octave_seed{seed + static_cast<uint32>(octave) * 0x9E3779B9u};
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
