#include "Generation/ImageGenerators.h"

namespace SandboxImages::GenLab {
namespace {
constexpr float inverse_uint24{1.0f / 16777215.0f};

auto smooth_step(float const edge0, float const edge1, float const value) -> float {
    if (edge0 == edge1) {
        return value < edge0 ? 0.0f : 1.0f;
    }

    auto const alpha{FMath::Clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f)};
    return alpha * alpha * (3.0f - 2.0f * alpha);
}

auto to_byte(float const value) -> uint8 {
    return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(value, 0.0f, 1.0f) * 255.0f));
}

auto grayscale(float const value, uint8 const alpha) -> FColor {
    auto const intensity{to_byte(value)};
    return {intensity, intensity, intensity, alpha};
}

auto grayscale_mask(float const value) -> FColor {
    auto const intensity{to_byte(value)};
    return {intensity, intensity, intensity, intensity};
}

auto make_image(int32 const width, int32 const height, FColor const fill) -> FGeneratedImage {
    FGeneratedImage image{.width = width, .height = height};
    if (width <= 0 || height <= 0 || width > MAX_int32 / height) {
        image.error = TEXT("Image dimensions must be positive and fit in an int32 pixel count.");
        return image;
    }

    image.pixels.Init(fill, width * height);
    return image;
}

auto invalid_image(FString const& error) -> FGeneratedImage {
    return {.error = error};
}

auto normalized_distance(int32 const x, int32 const y, int32 const width, int32 const height)
    -> float {
    auto const center_x{static_cast<float>(width - 1) * 0.5f};
    auto const center_y{static_cast<float>(height - 1) * 0.5f};
    auto const half_minimum_dimension{static_cast<float>(FMath::Min(width, height)) * 0.5f};
    auto const dx{(static_cast<float>(x) - center_x) / half_minimum_dimension};
    auto const dy{(static_cast<float>(y) - center_y) / half_minimum_dimension};
    return FMath::Sqrt(dx * dx + dy * dy);
}

class FDeterministicRandom {
  public:
    explicit FDeterministicRandom(uint32 const seed)
        : state_{seed} {}

    auto next_unit() -> float {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<float>(state_ >> 8u) * inverse_uint24;
    }
  private:
    uint32 state_;
};

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

auto lattice_value(int32 const x, int32 const y, uint32 const seed) -> float {
    return static_cast<float>(hash_lattice(x, y, seed) & 0x00FFFFFFu) * inverse_uint24;
}

auto value_noise(float const x, float const y, uint32 const seed) -> float {
    auto const x0{FMath::FloorToInt(x)};
    auto const y0{FMath::FloorToInt(y)};
    auto const tx{smooth_step(0.0f, 1.0f, x - static_cast<float>(x0))};
    auto const ty{smooth_step(0.0f, 1.0f, y - static_cast<float>(y0))};
    auto const top{FMath::Lerp(lattice_value(x0, y0, seed), lattice_value(x0 + 1, y0, seed), tx)};
    auto const bottom{
        FMath::Lerp(lattice_value(x0, y0 + 1, seed), lattice_value(x0 + 1, y0 + 1, seed), tx)};
    return FMath::Lerp(top, bottom, ty);
}

auto hex_signed_distance(float const x, float const y, float const radius) -> float {
    auto const absolute_x{FMath::Abs(x)};
    auto const absolute_y{FMath::Abs(y)};
    return FMath::Max(absolute_x * 0.8660254038f + absolute_y * 0.5f, absolute_y) - radius;
}
}

auto FGeneratedImage::is_valid() const -> bool {
    return width > 0 && height > 0 && width <= MAX_int32 / height && pixels.Num() == width * height;
}

auto generate_radial_gradient(FRadialGradientParameters const& parameters) -> FGeneratedImage {
    if (parameters.inner_radius < 0.0f || parameters.outer_radius <= parameters.inner_radius) {
        return invalid_image(TEXT("Radial radii must satisfy 0 <= inner_radius < outer_radius."));
    }

    auto image{make_image(parameters.width, parameters.height, FColor::Transparent)};
    if (!image.is_valid()) {
        return image;
    }

    for (int32 y{0}; y < parameters.height; ++y) {
        for (int32 x{0}; x < parameters.width; ++x) {
            auto const distance{normalized_distance(x, y, parameters.width, parameters.height)};
            auto const value{
                1.0f - smooth_step(parameters.inner_radius, parameters.outer_radius, distance)};
            image.pixels[y * parameters.width + x] = grayscale_mask(value);
        }
    }
    return image;
}

auto generate_ring_mask(FRingMaskParameters const& parameters) -> FGeneratedImage {
    if (parameters.radius < 0.0f || parameters.thickness <= 0.0f || parameters.falloff < 0.0f) {
        return invalid_image(
            TEXT("Ring radius and falloff must be non-negative and thickness must be positive."));
    }

    auto image{make_image(parameters.width, parameters.height, FColor::Transparent)};
    if (!image.is_valid()) {
        return image;
    }

    auto const half_thickness{parameters.thickness * 0.5f};
    for (int32 y{0}; y < parameters.height; ++y) {
        for (int32 x{0}; x < parameters.width; ++x) {
            auto const distance{normalized_distance(x, y, parameters.width, parameters.height)};
            auto const distance_from_ring{FMath::Abs(distance - parameters.radius)};
            auto const value{1.0f - smooth_step(half_thickness,
                                                half_thickness + parameters.falloff,
                                                distance_from_ring)};
            image.pixels[y * parameters.width + x] = grayscale_mask(value);
        }
    }
    return image;
}

auto generate_starfield(FStarfieldParameters const& parameters) -> FGeneratedImage {
    if (parameters.star_count < 0 || parameters.minimum_brightness < 0.0f ||
        parameters.minimum_brightness > 1.0f || parameters.minimum_radius <= 0.0f ||
        parameters.maximum_radius < parameters.minimum_radius) {
        return invalid_image(TEXT("Starfield parameters require a non-negative star count, "
                                  "brightness in [0, 1], and 0 < minimum_radius <= "
                                  "maximum_radius."));
    }

    auto const background{parameters.transparent_background ? FColor::Transparent : FColor::Black};
    auto image{make_image(parameters.width, parameters.height, background)};
    if (!image.is_valid()) {
        return image;
    }

    FDeterministicRandom random{parameters.seed};
    for (int32 star_index{0}; star_index < parameters.star_count; ++star_index) {
        auto const center_x{random.next_unit() * static_cast<float>(parameters.width - 1)};
        auto const center_y{random.next_unit() * static_cast<float>(parameters.height - 1)};
        auto const brightness{FMath::Lerp(parameters.minimum_brightness, 1.0f, random.next_unit())};
        auto const radius{
            FMath::Lerp(parameters.minimum_radius, parameters.maximum_radius, random.next_unit())};
        auto const minimum_x{FMath::Max(0, FMath::FloorToInt(center_x - radius - 1.0f))};
        auto const maximum_x{
            FMath::Min(parameters.width - 1, FMath::CeilToInt(center_x + radius + 1.0f))};
        auto const minimum_y{FMath::Max(0, FMath::FloorToInt(center_y - radius - 1.0f))};
        auto const maximum_y{
            FMath::Min(parameters.height - 1, FMath::CeilToInt(center_y + radius + 1.0f))};

        for (int32 y{minimum_y}; y <= maximum_y; ++y) {
            for (int32 x{minimum_x}; x <= maximum_x; ++x) {
                auto const dx{static_cast<float>(x) - center_x};
                auto const dy{static_cast<float>(y) - center_y};
                auto const distance{FMath::Sqrt(dx * dx + dy * dy)};
                auto const coverage{1.0f - smooth_step(radius * 0.2f, radius + 0.75f, distance)};
                auto const intensity{to_byte(brightness * coverage)};
                auto& pixel{image.pixels[y * parameters.width + x]};
                if (intensity > pixel.R) {
                    pixel = {intensity,
                             intensity,
                             intensity,
                             parameters.transparent_background ? intensity : uint8{255}};
                }
            }
        }
    }
    return image;
}

auto generate_noise(FNoiseParameters const& parameters) -> FGeneratedImage {
    if (parameters.base_scale <= 0.0f || parameters.octave_count <= 0 ||
        parameters.octave_count > 16 || parameters.persistence < 0.0f ||
        parameters.persistence > 1.0f) {
        return invalid_image(TEXT("Noise parameters require base_scale > 0, octave_count in "
                                  "[1, 16], and persistence in [0, 1]."));
    }

    auto image{make_image(parameters.width, parameters.height, FColor::Black)};
    if (!image.is_valid()) {
        return image;
    }

    for (int32 y{0}; y < parameters.height; ++y) {
        for (int32 x{0}; x < parameters.width; ++x) {
            float value{0.0f};
            float amplitude{1.0f};
            float total_amplitude{0.0f};
            float scale{parameters.base_scale};
            for (int32 octave{0}; octave < parameters.octave_count; ++octave) {
                auto const octave_seed{parameters.seed + static_cast<uint32>(octave) * 0x9E3779B9u};
                value += value_noise(static_cast<float>(x) / scale,
                                     static_cast<float>(y) / scale,
                                     octave_seed) *
                         amplitude;
                total_amplitude += amplitude;
                amplitude *= parameters.persistence;
                scale *= 0.5f;
            }

            auto const normalized_value{total_amplitude > 0.0f ? value / total_amplitude : 0.0f};
            image.pixels[y * parameters.width + x] = grayscale(normalized_value, 255);
        }
    }
    return image;
}

auto generate_hex_grid(FHexGridParameters const& parameters) -> FGeneratedImage {
    if (parameters.cell_radius <= 0.0f || parameters.line_thickness <= 0.0f ||
        parameters.falloff < 0.0f) {
        return invalid_image(TEXT("Hex grid parameters require positive cell radius and line "
                                  "thickness, with a non-negative falloff."));
    }

    auto image{make_image(parameters.width, parameters.height, FColor::Transparent)};
    if (!image.is_valid()) {
        return image;
    }

    auto const horizontal_spacing{FMath::Sqrt(3.0f) * parameters.cell_radius};
    auto const vertical_spacing{1.5f * parameters.cell_radius};
    auto const half_thickness{parameters.line_thickness * 0.5f};
    for (int32 y{0}; y < parameters.height; ++y) {
        auto const approximate_row{FMath::RoundToInt(static_cast<float>(y) / vertical_spacing)};
        for (int32 x{0}; x < parameters.width; ++x) {
            float nearest_edge{MAX_flt};
            for (int32 row{approximate_row - 2}; row <= approximate_row + 2; ++row) {
                auto const row_offset{(row & 1) != 0 ? horizontal_spacing * 0.5f : 0.0f};
                auto const approximate_column{
                    FMath::RoundToInt((static_cast<float>(x) - row_offset) / horizontal_spacing)};
                for (int32 column{approximate_column - 2}; column <= approximate_column + 2;
                     ++column) {
                    auto const center_x{static_cast<float>(column) * horizontal_spacing +
                                        row_offset};
                    auto const center_y{static_cast<float>(row) * vertical_spacing};
                    auto const distance{
                        FMath::Abs(hex_signed_distance(static_cast<float>(x) - center_x,
                                                       static_cast<float>(y) - center_y,
                                                       parameters.cell_radius))};
                    nearest_edge = FMath::Min(nearest_edge, distance);
                }
            }

            auto const value{1.0f - smooth_step(half_thickness,
                                                half_thickness + parameters.falloff,
                                                nearest_edge)};
            image.pixels[y * parameters.width + x] = grayscale_mask(value);
        }
    }
    return image;
}

}
