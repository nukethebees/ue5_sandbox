#include "Generation/ImageGenerators.h"
#include "Generation/ImageGenerationUtilities.h"
#include "Generation/NoiseSampling.h"

namespace SandboxImages::GenLab {
using namespace ImageGeneration;
using namespace NoiseSampling;

namespace {
void chamfer_distance_transform(TArray<float>& distances, int32 const width, int32 const height) {
    constexpr float diagonal_cost{1.4142135624f};
    auto update_distance{[&](int32 const x,
                             int32 const y,
                             int32 const neighbor_x,
                             int32 const neighbor_y,
                             float const cost) {
        if (neighbor_x < 0 || neighbor_x >= width || neighbor_y < 0 || neighbor_y >= height) {
            return;
        }
        auto& distance{distances[y * width + x]};
        distance = FMath::Min(distance, distances[neighbor_y * width + neighbor_x] + cost);
    }};

    for (int32 y{0}; y < height; ++y) {
        for (int32 x{0}; x < width; ++x) {
            update_distance(x, y, x - 1, y, 1.0f);
            update_distance(x, y, x, y - 1, 1.0f);
            update_distance(x, y, x - 1, y - 1, diagonal_cost);
            update_distance(x, y, x + 1, y - 1, diagonal_cost);
        }
    }
    for (int32 y{height - 1}; y >= 0; --y) {
        for (int32 x{width - 1}; x >= 0; --x) {
            update_distance(x, y, x + 1, y, 1.0f);
            update_distance(x, y, x, y + 1, 1.0f);
            update_distance(x, y, x + 1, y + 1, diagonal_cost);
            update_distance(x, y, x - 1, y + 1, diagonal_cost);
        }
    }
}

auto apply_post_process(FGeneratedImage image,
                        FImagePostProcessParameters const& parameters,
                        bool const alpha_is_intensity) -> FGeneratedImage {
    if (!image.is_valid()) {
        return image;
    }
    if (!FMath::IsFinite(parameters.contrast) || parameters.contrast < 0.0f) {
        return invalid_image(TEXT("Post-process contrast must be finite and non-negative."));
    }
    if (parameters.threshold_enabled &&
        (!FMath::IsFinite(parameters.threshold) || parameters.threshold < 0.0f ||
         parameters.threshold > 1.0f || !FMath::IsFinite(parameters.threshold_softness) ||
         parameters.threshold_softness < 0.0f || parameters.threshold_softness > 1.0f)) {
        return invalid_image(
            TEXT("Post-process threshold and threshold softness must be finite and in [0, 1]."));
    }
    if (parameters.output == FImagePostProcessParameters::EOutput::NormalMap &&
        (!FMath::IsFinite(parameters.normal_strength) || parameters.normal_strength < 0.0f)) {
        return invalid_image(TEXT("Normal-map strength must be finite and non-negative."));
    }
    if (parameters.output == FImagePostProcessParameters::EOutput::SignedDistance &&
        (!FMath::IsFinite(parameters.distance_threshold) || parameters.distance_threshold < 0.0f ||
         parameters.distance_threshold > 1.0f || !FMath::IsFinite(parameters.distance_range) ||
         parameters.distance_range < 1.0f || parameters.distance_range > 4096.0f)) {
        return invalid_image(
            TEXT("Signed-distance threshold must be in [0, 1] and its finite pixel "
                 "range must be in [1, 4096]."));
    }

    auto const transform{[&parameters](uint8 const channel) {
        auto value{static_cast<float>(channel) / 255.0f};
        value = (value - 0.5f) * parameters.contrast + 0.5f;
        if (parameters.threshold_enabled) {
            auto const half_softness{parameters.threshold_softness * 0.5f};
            value = parameters.threshold_softness > 0.0f
                      ? smooth_step(parameters.threshold - half_softness,
                                    parameters.threshold + half_softness,
                                    value)
                  : value >= parameters.threshold ? 1.0f
                                                  : 0.0f;
        }
        if (parameters.invert) {
            value = 1.0f - value;
        }
        return to_byte(value);
    }};
    for (auto& pixel : image.pixels) {
        pixel.R = transform(pixel.R);
        pixel.G = transform(pixel.G);
        pixel.B = transform(pixel.B);
        if (alpha_is_intensity) {
            pixel.A = transform(pixel.A);
        }
    }
    if (parameters.output == FImagePostProcessParameters::EOutput::NormalMap) {
        return generate_normal_map(image, parameters.normal_strength, parameters.normal_wrap);
    }
    if (parameters.output == FImagePostProcessParameters::EOutput::SignedDistance) {
        return generate_signed_distance_field(image,
                                              parameters.distance_threshold,
                                              parameters.distance_range,
                                              parameters.distance_wrap);
    }
    return image;
}

auto hex_signed_distance(float const x, float const y, float const radius) -> float {
    auto const absolute_x{FMath::Abs(x)};
    auto const absolute_y{FMath::Abs(y)};
    return FMath::Max(absolute_x * 0.8660254038f + absolute_y * 0.5f, absolute_y) - radius;
}
}

auto generate_normal_map(FGeneratedImage const& height_image, float const strength, bool const wrap)
    -> FGeneratedImage {
    if (!height_image.is_valid()) {
        return invalid_image(TEXT("Normal-map input must be a valid image."));
    }
    if (!FMath::IsFinite(strength) || strength < 0.0f) {
        return invalid_image(TEXT("Normal-map strength must be finite and non-negative."));
    }

    auto normal_map{make_image(height_image.width, height_image.height, FColor::Black)};
    auto const period_x{wrap && height_image.width > 1 ? height_image.width - 1
                                                       : height_image.width};
    auto const period_y{wrap && height_image.height > 1 ? height_image.height - 1
                                                        : height_image.height};
    auto const sample_height{[&](int32 x, int32 y) {
        if (wrap) {
            x = ((x % period_x) + period_x) % period_x;
            y = ((y % period_y) + period_y) % period_y;
        } else {
            x = FMath::Clamp(x, 0, height_image.width - 1);
            y = FMath::Clamp(y, 0, height_image.height - 1);
        }
        return static_cast<float>(height_image.pixels[y * height_image.width + x].R) / 255.0f;
    }};

    for (int32 y{0}; y < height_image.height; ++y) {
        auto const sample_y{wrap ? y % period_y : y};
        for (int32 x{0}; x < height_image.width; ++x) {
            auto const sample_x{wrap ? x % period_x : x};
            auto const height_left{sample_height(sample_x - 1, sample_y)};
            auto const height_right{sample_height(sample_x + 1, sample_y)};
            auto const height_up{sample_height(sample_x, sample_y - 1)};
            auto const height_down{sample_height(sample_x, sample_y + 1)};
            auto normal_x{-(height_right - height_left) * strength};
            auto normal_y{-(height_down - height_up) * strength};
            auto normal_z{1.0f};
            auto const inverse_length{
                FMath::InvSqrt(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z)};
            normal_x *= inverse_length;
            normal_y *= inverse_length;
            normal_z *= inverse_length;
            normal_map.pixels[y * height_image.width + x] = {to_byte(normal_x * 0.5f + 0.5f),
                                                             to_byte(normal_y * 0.5f + 0.5f),
                                                             to_byte(normal_z * 0.5f + 0.5f),
                                                             255};
        }
    }
    return normal_map;
}

auto generate_signed_distance_field(FGeneratedImage const& mask_image,
                                    float const threshold,
                                    float const distance_range,
                                    bool const wrap) -> FGeneratedImage {
    if (!mask_image.is_valid()) {
        return invalid_image(TEXT("Signed-distance input must be a valid image."));
    }
    if (!FMath::IsFinite(threshold) || threshold < 0.0f || threshold > 1.0f ||
        !FMath::IsFinite(distance_range) || distance_range < 1.0f || distance_range > 4096.0f) {
        return invalid_image(
            TEXT("Signed-distance threshold must be in [0, 1] and its finite pixel "
                 "range must be in [1, 4096]."));
    }

    auto const padding{wrap ? FMath::CeilToInt(distance_range) + 1 : 0};
    auto const padded_width64{static_cast<int64>(mask_image.width) + 2ll * padding};
    auto const padded_height64{static_cast<int64>(mask_image.height) + 2ll * padding};
    if (padded_width64 > MAX_int32 || padded_height64 > MAX_int32 ||
        padded_width64 * padded_height64 > MAX_int32) {
        return invalid_image(TEXT("Signed-distance padded dimensions exceed the supported size."));
    }
    auto const padded_width{static_cast<int32>(padded_width64)};
    auto const padded_height{static_cast<int32>(padded_height64)};
    auto const period_x{wrap && mask_image.width > 1 ? mask_image.width - 1 : mask_image.width};
    auto const period_y{wrap && mask_image.height > 1 ? mask_image.height - 1 : mask_image.height};
    auto const is_inside{[&](int32 x, int32 y) {
        if (wrap) {
            x = ((x % period_x) + period_x) % period_x;
            y = ((y % period_y) + period_y) % period_y;
        }
        return static_cast<float>(mask_image.pixels[y * mask_image.width + x].R) / 255.0f >=
               threshold;
    }};
    auto const build_distances{[&](bool const target_inside) {
        TArray<float> distances;
        distances.SetNumUninitialized(padded_width * padded_height);
        for (int32 y{0}; y < padded_height; ++y) {
            auto const source_y{y - padding};
            for (int32 x{0}; x < padded_width; ++x) {
                auto const source_x{x - padding};
                distances[y * padded_width + x] =
                    is_inside(source_x, source_y) == target_inside ? 0.0f : MAX_flt;
            }
        }
        chamfer_distance_transform(distances, padded_width, padded_height);
        return distances;
    }};

    auto const distance_to_inside{build_distances(true)};
    auto const distance_to_outside{build_distances(false)};
    auto distance_field{make_image(mask_image.width, mask_image.height, FColor::Black)};
    for (int32 y{0}; y < mask_image.height; ++y) {
        auto const sample_y{padding + (wrap ? y % period_y : y)};
        for (int32 x{0}; x < mask_image.width; ++x) {
            auto const sample_x{padding + (wrap ? x % period_x : x)};
            auto const padded_index{sample_y * padded_width + sample_x};
            auto const inside{is_inside(x, y)};
            auto const nearest_opposite_distance{FMath::Min(
                inside ? distance_to_outside[padded_index] : distance_to_inside[padded_index],
                distance_range)};
            auto const signed_distance{inside ? nearest_opposite_distance - 0.5f
                                              : 0.5f - nearest_opposite_distance};
            auto const value{0.5f + signed_distance / (2.0f * distance_range)};
            distance_field.pixels[y * mask_image.width + x] = grayscale(value, 255);
        }
    }
    return distance_field;
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

auto generate_shockwave_flipbook(FShockwaveFlipbookParameters const& parameters)
    -> FGeneratedImage {
    if (parameters.columns <= 0 || parameters.rows <= 0 || parameters.frame_count <= 0 ||
        parameters.columns > MAX_int32 / parameters.rows ||
        parameters.frame_count > parameters.columns * parameters.rows) {
        return invalid_image(
            TEXT("Flipbook grid dimensions must be positive and contain every frame."));
    }
    if (parameters.width <= 0 || parameters.height <= 0 ||
        parameters.width % parameters.columns != 0 || parameters.height % parameters.rows != 0) {
        return invalid_image(
            TEXT("Flipbook image dimensions must be positive and divisible by the grid."));
    }
    if (parameters.start_radius < 0.0f || parameters.end_radius < parameters.start_radius ||
        parameters.thickness <= 0.0f || parameters.falloff < 0.0f ||
        parameters.start_intensity < 0.0f || parameters.start_intensity > 1.0f ||
        parameters.end_intensity < 0.0f || parameters.end_intensity > 1.0f) {
        return invalid_image(
            TEXT("Shockwave radii must increase, thickness must be positive, "
                 "falloff must be non-negative, and intensities must be in [0, 1]."));
    }

    auto image{make_image(parameters.width, parameters.height, FColor::Transparent)};
    if (!image.is_valid()) {
        return image;
    }

    auto const frame_width{parameters.width / parameters.columns};
    auto const frame_height{parameters.height / parameters.rows};
    auto const half_thickness{parameters.thickness * 0.5f};
    for (int32 frame{0}; frame < parameters.frame_count; ++frame) {
        auto const progress{parameters.frame_count > 1
                                ? static_cast<float>(frame) /
                                      static_cast<float>(parameters.frame_count - 1)
                                : 0.0f};
        auto const radius{FMath::Lerp(parameters.start_radius, parameters.end_radius, progress)};
        auto const intensity{
            FMath::Lerp(parameters.start_intensity, parameters.end_intensity, progress)};
        auto const frame_x{frame % parameters.columns};
        auto const frame_y{frame / parameters.columns};
        auto const origin_x{frame_x * frame_width};
        auto const origin_y{frame_y * frame_height};

        for (int32 local_y{0}; local_y < frame_height; ++local_y) {
            for (int32 local_x{0}; local_x < frame_width; ++local_x) {
                auto const distance{
                    normalized_distance(local_x, local_y, frame_width, frame_height)};
                auto const distance_from_ring{FMath::Abs(distance - radius)};
                auto const coverage{1.0f - smooth_step(half_thickness,
                                                       half_thickness + parameters.falloff,
                                                       distance_from_ring)};
                image.pixels[(origin_y + local_y) * parameters.width + origin_x + local_x] =
                    grayscale_mask(coverage * intensity);
            }
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
            auto const value{fractal_noise_sample(static_cast<float>(x),
                                                  static_cast<float>(y),
                                                  parameters.width,
                                                  parameters.height,
                                                  parameters.seed,
                                                  parameters.base_scale,
                                                  parameters.octave_count,
                                                  parameters.persistence,
                                                  parameters.tileable)};
            image.pixels[y * parameters.width + x] = grayscale(value, 255);
        }
    }
    return image;
}

auto generate_domain_warped_noise(FDomainWarpedNoiseParameters const& parameters)
    -> FGeneratedImage {
    if (parameters.base_scale <= 0.0f || parameters.warp_scale <= 0.0f ||
        parameters.warp_strength < 0.0f || parameters.base_octave_count <= 0 ||
        parameters.base_octave_count > 16 || parameters.warp_octave_count <= 0 ||
        parameters.warp_octave_count > 16 || parameters.persistence < 0.0f ||
        parameters.persistence > 1.0f) {
        return invalid_image(TEXT("Domain-warped noise requires positive base and warp scales, a "
                                  "non-negative warp strength, octave counts in [1, 16], and "
                                  "persistence in [0, 1]."));
    }

    auto image{make_image(parameters.width, parameters.height, FColor::Black)};
    if (!image.is_valid()) {
        return image;
    }

    for (int32 y{0}; y < parameters.height; ++y) {
        for (int32 x{0}; x < parameters.width; ++x) {
            auto const warp_x{fractal_noise_sample(static_cast<float>(x),
                                                   static_cast<float>(y),
                                                   parameters.width,
                                                   parameters.height,
                                                   parameters.warp_seed,
                                                   parameters.warp_scale,
                                                   parameters.warp_octave_count,
                                                   parameters.persistence,
                                                   parameters.tileable) *
                                  2.0f -
                              1.0f};
            auto const warp_y{fractal_noise_sample(static_cast<float>(x),
                                                   static_cast<float>(y),
                                                   parameters.width,
                                                   parameters.height,
                                                   parameters.warp_seed ^ 0xA511E9B3u,
                                                   parameters.warp_scale,
                                                   parameters.warp_octave_count,
                                                   parameters.persistence,
                                                   parameters.tileable) *
                                  2.0f -
                              1.0f};
            auto const sample_x{static_cast<float>(x) + warp_x * parameters.warp_strength};
            auto const sample_y{static_cast<float>(y) + warp_y * parameters.warp_strength};
            auto const value{fractal_noise_sample(sample_x,
                                                  sample_y,
                                                  parameters.width,
                                                  parameters.height,
                                                  parameters.base_seed,
                                                  parameters.base_scale,
                                                  parameters.base_octave_count,
                                                  parameters.persistence,
                                                  parameters.tileable)};
            image.pixels[y * parameters.width + x] = grayscale(value, 255);
        }
    }
    return image;
}

auto generate_curl_noise_flow(FCurlNoiseFlowParameters const& parameters) -> FGeneratedImage {
    if (!FMath::IsFinite(parameters.base_scale) || parameters.base_scale <= 0.0f ||
        parameters.octave_count <= 0 || parameters.octave_count > 16 ||
        !FMath::IsFinite(parameters.persistence) || parameters.persistence < 0.0f ||
        parameters.persistence > 1.0f || !FMath::IsFinite(parameters.derivative_step) ||
        parameters.derivative_step <= 0.0f || !FMath::IsFinite(parameters.strength) ||
        parameters.strength < 0.0f || parameters.strength > 1.0f) {
        return invalid_image(TEXT("Curl-noise flow parameters require a positive finite scale and "
                                  "derivative step, octave_count in [1, 16], and persistence and "
                                  "strength in [0, 1]."));
    }

    auto image{make_image(parameters.width, parameters.height, FColor{128, 128, 128, 255})};
    if (!image.is_valid()) {
        return image;
    }

    auto const sample{[&parameters](float const x, float const y) {
        return fractal_noise_sample(x,
                                    y,
                                    parameters.width,
                                    parameters.height,
                                    parameters.seed,
                                    parameters.base_scale,
                                    parameters.octave_count,
                                    parameters.persistence,
                                    parameters.tileable);
    }};
    for (int32 y{0}; y < parameters.height; ++y) {
        for (int32 x{0}; x < parameters.width; ++x) {
            auto const sample_x{static_cast<float>(x)};
            auto const sample_y{static_cast<float>(y)};
            auto flow_x{sample(sample_x, sample_y + parameters.derivative_step) -
                        sample(sample_x, sample_y - parameters.derivative_step)};
            auto flow_y{sample(sample_x - parameters.derivative_step, sample_y) -
                        sample(sample_x + parameters.derivative_step, sample_y)};
            auto const magnitude{FMath::Sqrt(flow_x * flow_x + flow_y * flow_y)};
            if (magnitude > UE_SMALL_NUMBER) {
                auto const scale{parameters.strength / magnitude};
                flow_x *= scale;
                flow_y *= scale;
            } else {
                flow_x = 0.0f;
                flow_y = 0.0f;
            }

            image.pixels[y * parameters.width + x] = {
                to_byte(flow_x * 0.5f + 0.5f), to_byte(flow_y * 0.5f + 0.5f), 128, 255};
        }
    }
    return image;
}

auto generate_cellular_noise(FCellularNoiseParameters const& parameters) -> FGeneratedImage {
    if (!FMath::IsFinite(parameters.cell_size) || parameters.cell_size <= 0.0f ||
        !FMath::IsFinite(parameters.jitter) || parameters.jitter < 0.0f ||
        parameters.jitter > 1.0f || !FMath::IsFinite(parameters.edge_width) ||
        parameters.edge_width < 0.0f || !FMath::IsFinite(parameters.falloff) ||
        parameters.falloff < 0.0f) {
        return invalid_image(TEXT("Cellular noise requires a positive finite cell size, jitter in "
                                  "[0, 1], and non-negative finite edge width and falloff."));
    }

    auto image{make_image(parameters.width, parameters.height, FColor::Black)};
    if (!image.is_valid()) {
        return image;
    }

    auto const cell_count_x{FMath::Max(
        1, FMath::RoundToInt(static_cast<float>(parameters.width) / parameters.cell_size))};
    auto const cell_count_y{FMath::Max(
        1, FMath::RoundToInt(static_cast<float>(parameters.height) / parameters.cell_size))};
    auto const effective_cell_size{
        parameters.tileable
            ? FMath::Min(
                  parameters.width > 1 ? static_cast<float>(parameters.width - 1) / cell_count_x
                                       : parameters.cell_size,
                  parameters.height > 1 ? static_cast<float>(parameters.height - 1) / cell_count_y
                                        : parameters.cell_size)
            : parameters.cell_size};
    for (int32 y{0}; y < parameters.height; ++y) {
        auto const sample_y{parameters.tileable && parameters.height > 1
                                ? static_cast<float>(y) / (parameters.height - 1) * cell_count_y
                                : static_cast<float>(y) / parameters.cell_size};
        auto const base_cell_y{FMath::FloorToInt(sample_y)};
        for (int32 x{0}; x < parameters.width; ++x) {
            auto const sample_x{parameters.tileable && parameters.width > 1
                                    ? static_cast<float>(x) / (parameters.width - 1) * cell_count_x
                                    : static_cast<float>(x) / parameters.cell_size};
            auto const base_cell_x{FMath::FloorToInt(sample_x)};
            float nearest_distance_squared{MAX_flt};
            float second_distance_squared{MAX_flt};
            for (int32 candidate_y{base_cell_y - 2}; candidate_y <= base_cell_y + 2;
                 ++candidate_y) {
                for (int32 candidate_x{base_cell_x - 2}; candidate_x <= base_cell_x + 2;
                     ++candidate_x) {
                    auto const hash_x{parameters.tileable
                                          ? ((candidate_x % cell_count_x) + cell_count_x) %
                                                cell_count_x
                                          : candidate_x};
                    auto const hash_y{parameters.tileable
                                          ? ((candidate_y % cell_count_y) + cell_count_y) %
                                                cell_count_y
                                          : candidate_y};
                    auto const offset_x{(lattice_value(hash_x, hash_y, parameters.seed) - 0.5f) *
                                        parameters.jitter};
                    auto const offset_y{
                        (lattice_value(hash_x, hash_y, parameters.seed ^ 0xA511E9B3u) - 0.5f) *
                        parameters.jitter};
                    auto const dx{static_cast<float>(candidate_x) + 0.5f + offset_x - sample_x};
                    auto const dy{static_cast<float>(candidate_y) + 0.5f + offset_y - sample_y};
                    auto const distance_squared{dx * dx + dy * dy};
                    if (distance_squared < nearest_distance_squared) {
                        second_distance_squared = nearest_distance_squared;
                        nearest_distance_squared = distance_squared;
                    } else if (distance_squared < second_distance_squared) {
                        second_distance_squared = distance_squared;
                    }
                }
            }

            float value{};
            if (parameters.mode == ECellularMode::Distance) {
                value =
                    FMath::Clamp(FMath::Sqrt(nearest_distance_squared) * 0.7071067812f, 0.0f, 1.0f);
            } else {
                auto const edge_distance{
                    (FMath::Sqrt(second_distance_squared) - FMath::Sqrt(nearest_distance_squared)) *
                    effective_cell_size};
                value = 1.0f - smooth_step(parameters.edge_width,
                                           parameters.edge_width + parameters.falloff,
                                           edge_distance);
            }
            image.pixels[y * parameters.width + x] = grayscale(value, 255);
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

auto make_default_request(EGeneratorType const generator) -> FGenerationRequest {
    FGenerationRequest request{};
    request.generator = generator;
    switch (generator) {
        case EGeneratorType::RadialGradient:
            request.output_name = TEXT("soft_radial_gradient");
            break;
        case EGeneratorType::RingMask:
            request.output_name = TEXT("ring_mask");
            break;
        case EGeneratorType::ShockwaveFlipbook:
            request.output_name = TEXT("shockwave_flipbook");
            break;
        case EGeneratorType::Starfield:
            request.output_name = TEXT("starfield");
            break;
        case EGeneratorType::Noise:
            request.output_name = TEXT("coherent_noise");
            break;
        case EGeneratorType::DomainWarpedNoise:
            request.output_name = TEXT("nebula_soft");
            break;
        case EGeneratorType::CurlNoiseFlow:
            request.output_name = TEXT("nebula_flow");
            break;
        case EGeneratorType::CellularNoise:
            request.output_name = TEXT("cellular_regions");
            break;
        case EGeneratorType::HexGrid:
            request.output_name = TEXT("hex_grid_mask");
            break;
    }
    return request;
}

auto default_generation_requests() -> TArray<FGenerationRequest> {
    auto ring_distance{make_default_request(EGeneratorType::RingMask)};
    ring_distance.output_name = TEXT("ring_distance");
    ring_distance.post_process = {.output = FImagePostProcessParameters::EOutput::SignedDistance,
                                  .distance_threshold = 0.5f,
                                  .distance_range = 24.0f,
                                  .distance_wrap = false};

    auto nebula_soft{make_default_request(EGeneratorType::DomainWarpedNoise)};

    auto nebula_normal{nebula_soft};
    nebula_normal.output_name = TEXT("nebula_normal");
    nebula_normal.post_process = {.output = FImagePostProcessParameters::EOutput::NormalMap,
                                  .normal_strength = 10.0f,
                                  .normal_wrap = true};

    auto energy_filaments{make_default_request(EGeneratorType::DomainWarpedNoise)};
    energy_filaments.output_name = TEXT("energy_filaments");
    energy_filaments.domain_warped_noise = {.base_seed = 0x454E4552u,
                                            .warp_seed = 0x46494C41u,
                                            .base_scale = 38.0f,
                                            .warp_scale = 72.0f,
                                            .warp_strength = 52.0f,
                                            .base_octave_count = 6,
                                            .warp_octave_count = 3,
                                            .persistence = 0.55f,
                                            .tileable = true};
    energy_filaments.post_process = {.contrast = 1.8f,
                                     .threshold_enabled = true,
                                     .threshold = 0.53f,
                                     .threshold_softness = 0.22f};

    auto shield_turbulence{make_default_request(EGeneratorType::DomainWarpedNoise)};
    shield_turbulence.output_name = TEXT("shield_turbulence");
    shield_turbulence.domain_warped_noise = {.base_seed = 0x53484945u,
                                             .warp_seed = 0x54555242u,
                                             .base_scale = 52.0f,
                                             .warp_scale = 108.0f,
                                             .warp_strength = 68.0f,
                                             .base_octave_count = 5,
                                             .warp_octave_count = 4,
                                             .persistence = 0.6f,
                                             .tileable = true};
    shield_turbulence.post_process = {.contrast = 1.35f,
                                      .threshold_enabled = true,
                                      .threshold = 0.48f,
                                      .threshold_softness = 0.38f};

    auto nebula_flow{make_default_request(EGeneratorType::CurlNoiseFlow)};

    auto shield_distortion_flow{make_default_request(EGeneratorType::CurlNoiseFlow)};
    shield_distortion_flow.output_name = TEXT("shield_distortion_flow");
    shield_distortion_flow.curl_noise_flow = {.seed = 0x53484945u,
                                              .base_scale = 64.0f,
                                              .octave_count = 4,
                                              .persistence = 0.35f,
                                              .derivative_step = 2.0f,
                                              .strength = 0.85f,
                                              .tileable = true};

    auto cellular_regions{make_default_request(EGeneratorType::CellularNoise)};

    auto shield_cells{make_default_request(EGeneratorType::CellularNoise)};
    shield_cells.output_name = TEXT("shield_cells");
    shield_cells.cellular_noise = {.seed = 0x53484945u,
                                   .cell_size = 28.0f,
                                   .jitter = 0.35f,
                                   .mode = ECellularMode::Borders,
                                   .edge_width = 1.5f,
                                   .falloff = 1.5f,
                                   .tileable = true};

    auto shield_cells_normal{shield_cells};
    shield_cells_normal.output_name = TEXT("shield_cells_normal");
    shield_cells_normal.post_process = {.output = FImagePostProcessParameters::EOutput::NormalMap,
                                        .normal_strength = 6.0f,
                                        .normal_wrap = true};

    auto shield_cells_distance{shield_cells};
    shield_cells_distance.output_name = TEXT("shield_cells_distance");
    shield_cells_distance.post_process = {.output =
                                              FImagePostProcessParameters::EOutput::SignedDistance,
                                          .distance_threshold = 0.5f,
                                          .distance_range = 12.0f,
                                          .distance_wrap = true};

    auto fracture_edges{make_default_request(EGeneratorType::CellularNoise)};
    fracture_edges.output_name = TEXT("fracture_edges");
    fracture_edges.cellular_noise = {.seed = 0x46524143u,
                                     .cell_size = 36.0f,
                                     .jitter = 1.0f,
                                     .mode = ECellularMode::Borders,
                                     .edge_width = 0.75f,
                                     .falloff = 1.25f,
                                     .tileable = true};

    return {
        make_default_request(EGeneratorType::RadialGradient),
        make_default_request(EGeneratorType::RingMask),
        MoveTemp(ring_distance),
        make_default_request(EGeneratorType::ShockwaveFlipbook),
        make_default_request(EGeneratorType::Starfield),
        make_default_request(EGeneratorType::Noise),
        MoveTemp(nebula_soft),
        MoveTemp(nebula_normal),
        MoveTemp(energy_filaments),
        MoveTemp(shield_turbulence),
        MoveTemp(nebula_flow),
        MoveTemp(shield_distortion_flow),
        MoveTemp(cellular_regions),
        MoveTemp(shield_cells),
        MoveTemp(shield_cells_normal),
        MoveTemp(shield_cells_distance),
        MoveTemp(fracture_edges),
        make_default_request(EGeneratorType::HexGrid),
    };
}

auto generate_image(FGenerationRequest const& request) -> FGeneratedImage {
    FGeneratedImage image;
    bool alpha_is_intensity{false};
    switch (request.generator) {
        case EGeneratorType::RadialGradient:
            image = generate_radial_gradient(request.radial_gradient);
            alpha_is_intensity = true;
            break;
        case EGeneratorType::RingMask:
            image = generate_ring_mask(request.ring_mask);
            alpha_is_intensity = true;
            break;
        case EGeneratorType::ShockwaveFlipbook:
            image = generate_shockwave_flipbook(request.shockwave_flipbook);
            alpha_is_intensity = true;
            break;
        case EGeneratorType::Starfield:
            image = generate_starfield(request.starfield);
            alpha_is_intensity = request.starfield.transparent_background;
            break;
        case EGeneratorType::Noise:
            image = generate_noise(request.noise);
            break;
        case EGeneratorType::DomainWarpedNoise:
            image = generate_domain_warped_noise(request.domain_warped_noise);
            break;
        case EGeneratorType::CurlNoiseFlow:
            image = generate_curl_noise_flow(request.curl_noise_flow);
            break;
        case EGeneratorType::CellularNoise:
            image = generate_cellular_noise(request.cellular_noise);
            break;
        case EGeneratorType::HexGrid:
            image = generate_hex_grid(request.hex_grid);
            alpha_is_intensity = true;
            break;
    }
    if (request.generator == EGeneratorType::CurlNoiseFlow) {
        return image;
    }
    return apply_post_process(MoveTemp(image), request.post_process, alpha_is_intensity);
}

auto describe_request(FGenerationRequest const& request) -> FString {
    FString description;
    switch (request.generator) {
        case EGeneratorType::RadialGradient:
            description = FString::Printf(TEXT("version=6; generator=radial_gradient; "
                                               "dimensions=%dx%d; inner_radius=%g; "
                                               "outer_radius=%g"),
                                          request.radial_gradient.width,
                                          request.radial_gradient.height,
                                          request.radial_gradient.inner_radius,
                                          request.radial_gradient.outer_radius);
            break;
        case EGeneratorType::RingMask:
            description =
                FString::Printf(TEXT("version=6; generator=ring_mask; dimensions=%dx%d; radius=%g; "
                                     "thickness=%g; falloff=%g"),
                                request.ring_mask.width,
                                request.ring_mask.height,
                                request.ring_mask.radius,
                                request.ring_mask.thickness,
                                request.ring_mask.falloff);
            break;
        case EGeneratorType::ShockwaveFlipbook:
            description = FString::Printf(
                TEXT("version=6; generator=shockwave_flipbook; dimensions=%dx%d; grid=%dx%d; "
                     "frame_count=%d; start_radius=%g; end_radius=%g; thickness=%g; falloff=%g; "
                     "start_intensity=%g; end_intensity=%g"),
                request.shockwave_flipbook.width,
                request.shockwave_flipbook.height,
                request.shockwave_flipbook.columns,
                request.shockwave_flipbook.rows,
                request.shockwave_flipbook.frame_count,
                request.shockwave_flipbook.start_radius,
                request.shockwave_flipbook.end_radius,
                request.shockwave_flipbook.thickness,
                request.shockwave_flipbook.falloff,
                request.shockwave_flipbook.start_intensity,
                request.shockwave_flipbook.end_intensity);
            break;
        case EGeneratorType::Starfield:
            description = FString::Printf(
                TEXT("version=6; generator=starfield; dimensions=%dx%d; seed=%u; "
                     "star_count=%d; minimum_brightness=%g; minimum_radius=%g; "
                     "maximum_radius=%g; transparent_background=%s"),
                request.starfield.width,
                request.starfield.height,
                request.starfield.seed,
                request.starfield.star_count,
                request.starfield.minimum_brightness,
                request.starfield.minimum_radius,
                request.starfield.maximum_radius,
                request.starfield.transparent_background ? TEXT("true") : TEXT("false"));
            break;
        case EGeneratorType::Noise:
            description = FString::Printf(
                TEXT("version=6; generator=noise; dimensions=%dx%d; seed=%u; base_scale=%g; "
                     "octave_count=%d; persistence=%g; tileable=%s"),
                request.noise.width,
                request.noise.height,
                request.noise.seed,
                request.noise.base_scale,
                request.noise.octave_count,
                request.noise.persistence,
                request.noise.tileable ? TEXT("true") : TEXT("false"));
            break;
        case EGeneratorType::DomainWarpedNoise:
            description = FString::Printf(
                TEXT("version=6; generator=domain_warped_noise; dimensions=%dx%d; base_seed=%u; "
                     "warp_seed=%u; base_scale=%g; warp_scale=%g; warp_strength=%g; "
                     "base_octave_count=%d; warp_octave_count=%d; persistence=%g; tileable=%s"),
                request.domain_warped_noise.width,
                request.domain_warped_noise.height,
                request.domain_warped_noise.base_seed,
                request.domain_warped_noise.warp_seed,
                request.domain_warped_noise.base_scale,
                request.domain_warped_noise.warp_scale,
                request.domain_warped_noise.warp_strength,
                request.domain_warped_noise.base_octave_count,
                request.domain_warped_noise.warp_octave_count,
                request.domain_warped_noise.persistence,
                request.domain_warped_noise.tileable ? TEXT("true") : TEXT("false"));
            break;
        case EGeneratorType::CurlNoiseFlow:
            description = FString::Printf(
                TEXT("version=6; generator=curl_noise_flow; dimensions=%dx%d; seed=%u; "
                     "base_scale=%g; octave_count=%d; persistence=%g; derivative_step=%g; "
                     "strength=%g; tileable=%s"),
                request.curl_noise_flow.width,
                request.curl_noise_flow.height,
                request.curl_noise_flow.seed,
                request.curl_noise_flow.base_scale,
                request.curl_noise_flow.octave_count,
                request.curl_noise_flow.persistence,
                request.curl_noise_flow.derivative_step,
                request.curl_noise_flow.strength,
                request.curl_noise_flow.tileable ? TEXT("true") : TEXT("false"));
            break;
        case EGeneratorType::CellularNoise:
            description = FString::Printf(
                TEXT("version=6; generator=cellular_noise; dimensions=%dx%d; seed=%u; "
                     "cell_size=%g; jitter=%g; mode=%s; edge_width=%g; falloff=%g; tileable=%s"),
                request.cellular_noise.width,
                request.cellular_noise.height,
                request.cellular_noise.seed,
                request.cellular_noise.cell_size,
                request.cellular_noise.jitter,
                request.cellular_noise.mode == ECellularMode::Distance ? TEXT("distance")
                                                                       : TEXT("borders"),
                request.cellular_noise.edge_width,
                request.cellular_noise.falloff,
                request.cellular_noise.tileable ? TEXT("true") : TEXT("false"));
            break;
        case EGeneratorType::HexGrid:
            description = FString::Printf(TEXT("version=6; generator=hex_grid; dimensions=%dx%d; "
                                               "cell_radius=%g; line_thickness=%g; falloff=%g"),
                                          request.hex_grid.width,
                                          request.hex_grid.height,
                                          request.hex_grid.cell_radius,
                                          request.hex_grid.line_thickness,
                                          request.hex_grid.falloff);
            break;
    }
    if (request.generator == EGeneratorType::CurlNoiseFlow) {
        return description;
    }
    TCHAR const* output_name{};
    switch (request.post_process.output) {
        case FImagePostProcessParameters::EOutput::Scalar:
            output_name = TEXT("scalar");
            break;
        case FImagePostProcessParameters::EOutput::NormalMap:
            output_name = TEXT("normal_map");
            break;
        case FImagePostProcessParameters::EOutput::SignedDistance:
            output_name = TEXT("signed_distance");
            break;
    }
    return description +
           FString::Printf(TEXT("; invert=%s; contrast=%g; threshold_enabled=%s; threshold=%g; "
                                "threshold_softness=%g; output=%s; normal_strength=%g; "
                                "normal_wrap=%s; distance_threshold=%g; distance_range=%g; "
                                "distance_wrap=%s"),
                           request.post_process.invert ? TEXT("true") : TEXT("false"),
                           request.post_process.contrast,
                           request.post_process.threshold_enabled ? TEXT("true") : TEXT("false"),
                           request.post_process.threshold,
                           request.post_process.threshold_softness,
                           output_name,
                           request.post_process.normal_strength,
                           request.post_process.normal_wrap ? TEXT("true") : TEXT("false"),
                           request.post_process.distance_threshold,
                           request.post_process.distance_range,
                           request.post_process.distance_wrap ? TEXT("true") : TEXT("false"));
}

}
