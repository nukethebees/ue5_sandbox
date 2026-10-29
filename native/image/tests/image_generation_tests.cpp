#include <ioj/testing/structural_diff.h>

#include "sandbox/image/image_generation.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

namespace sandbox::image {
void diff_members(Pixel const& expected, Pixel const& actual, ioj::DiffContext& context) {
    context.diff("red", expected.red, actual.red);
    context.diff("green", expected.green, actual.green);
    context.diff("blue", expected.blue, actual.blue);
    context.diff("alpha", expected.alpha, actual.alpha);
}

void diff_members(GeneratedImage const& expected,
                  GeneratedImage const& actual,
                  ioj::DiffContext& context) {
    context.diff("width", expected.width, actual.width);
    context.diff("height", expected.height, actual.height);
    context.diff("pixels", expected.pixels, actual.pixels);
    context.diff("error", expected.error, actual.error);
}
}

namespace sandbox::image::tests {
using ioj::testing::structurally_equal;

auto pixel_at(GeneratedImage const& image, std::int32_t x, std::int32_t y) -> Pixel const& {
    return image.pixels[static_cast<std::size_t>(y * image.width + x)];
}

void expect_valid_image(GeneratedImage const& image, std::int32_t width, std::int32_t height) {
    ASSERT_TRUE(image.is_valid()) << image.error;
    EXPECT_EQ(image.width, width);
    EXPECT_EQ(image.height, height);
    EXPECT_EQ(image.pixels.size(), static_cast<std::size_t>(width * height));
}

void expect_matching_edges(GeneratedImage const& image) {
    ASSERT_TRUE(image.is_valid()) << image.error;
    for (std::int32_t y{}; y < image.height; ++y) {
        EXPECT_EQ(pixel_at(image, 0, y), pixel_at(image, image.width - 1, y)) << "row " << y;
    }
    for (std::int32_t x{}; x < image.width; ++x) {
        EXPECT_EQ(pixel_at(image, x, 0), pixel_at(image, x, image.height - 1)) << "column " << x;
    }
}

auto expect_seeded(auto generate, auto& parameters, auto seed_member, std::uint32_t changed_seed)
    -> GeneratedImage {
    auto first{generate(parameters)};
    EXPECT_PRED_FORMAT2(structurally_equal, first, generate(parameters));
    parameters.*seed_member = changed_seed;
    auto const changed{generate(parameters)};
    EXPECT_NE(first.pixels, changed.pixels);
    return first;
}

auto pixel_checksum(GeneratedImage const& image) -> std::uint32_t {
    std::uint32_t checksum{2166136261u};
    for (auto const& pixel : image.pixels) {
        for (auto const channel : {pixel.red, pixel.green, pixel.blue, pixel.alpha}) {
            checksum = (checksum ^ channel) * 16777619u;
        }
    }
    return checksum;
}

struct GenerationCase {
    char const* name;
    GeneratedImage (*generate)();
    bool valid{true};
};

GenerationCase const generation_cases[]{
    {"RadialGradient", [] { return generate_radial_gradient({.width = 48, .height = 32}); }},
    {"RingMask", [] { return generate_ring_mask({.width = 48, .height = 32}); }},
    {"ShockwaveFlipbook", [] { return generate_shockwave_flipbook({.width = 48, .height = 32}); }},
    {"Starfield", [] { return generate_starfield({.width = 48, .height = 32}); }},
    {"Noise", [] { return generate_noise({.width = 48, .height = 32}); }},
    {"DomainWarpedNoise", [] { return generate_domain_warped_noise({.width = 48, .height = 32}); }},
    {"CurlNoiseFlow", [] { return generate_curl_noise_flow({.width = 48, .height = 32}); }},
    {"CellularNoise", [] { return generate_cellular_noise({.width = 48, .height = 32}); }},
    {"HexGrid", [] { return generate_hex_grid({.width = 48, .height = 32}); }},
    {"SignedDistance",
     [] {
         return generate_signed_distance_field(
             generate_ring_mask({.width = 48, .height = 32}), 0.5f, 8.0f, false);
     }},
    {"InvalidDimensions", [] { return generate_radial_gradient({.width = 0}); }, false},
    {"InvalidRingThickness", [] { return generate_ring_mask({.thickness = 0.0f}); }, false},
    {"InvalidFlipbookWidth", [] { return generate_shockwave_flipbook({.width = 255}); }, false},
    {"InvalidStarRadius", [] { return generate_starfield({.minimum_radius = -1.0f}); }, false},
    {"InvalidNoiseScale", [] { return generate_noise({.base_scale = 0.0f}); }, false},
    {"InvalidWarpOctaves",
     [] { return generate_domain_warped_noise({.warp_octave_count = 0}); },
     false},
    {"InvalidDerivativeStep",
     [] { return generate_curl_noise_flow({.derivative_step = 0.0f}); },
     false},
    {"InvalidCellularJitter", [] { return generate_cellular_noise({.jitter = 2.0f}); }, false},
    {"InvalidHexRadius", [] { return generate_hex_grid({.cell_radius = 0.0f}); }, false},
    {"InvalidNormalStrength",
     [] {
         auto request{make_default_request(GeneratorType::Noise)};
         request.post_process = {.output = ImagePostProcessParameters::Output::NormalMap,
                                 .normal_strength = -1.0f};
         return generate_image(request);
     },
     false},
    {"InvalidDistanceRange",
     [] {
         auto request{make_default_request(GeneratorType::RingMask)};
         request.post_process = {.output = ImagePostProcessParameters::Output::SignedDistance,
                                 .distance_range = 0.0f};
         return generate_image(request);
     },
     false},
    {"InvalidContrast",
     [] {
         auto request{make_default_request(GeneratorType::RadialGradient)};
         request.radial_gradient = {.width = 65, .height = 65};
         request.post_process.contrast = -1.0f;
         return generate_image(request);
     },
     false},
};

class ImageGeneratorBuffers : public testing::TestWithParam<GenerationCase> {};

TEST_P(ImageGeneratorBuffers, ValidatesInputAndProducesCompleteBuffers) {
    auto const& test{GetParam()};
    auto const image{test.generate()};
    if (test.valid) {
        expect_valid_image(image, 48, 32);
    } else {
        EXPECT_FALSE(image.is_valid());
        EXPECT_FALSE(image.error.empty());
    }
}

INSTANTIATE_TEST_SUITE_P(Generators,
                         ImageGeneratorBuffers,
                         testing::ValuesIn(generation_cases),
                         [](auto const& info) { return info.param.name; });

struct ChecksumCase {
    char const* name;
    std::string_view output_name;
    std::uint32_t expected_checksum;
};

constexpr ChecksumCase checksum_cases[]{
    {"NebulaSoft", "nebula_soft", 1376257265u},
    {"EnergyFilaments", "energy_filaments", 2302176986u},
    {"ShieldTurbulence", "shield_turbulence", 3121887609u},
    {"NebulaFlow", "nebula_flow", 3933359053u},
    {"ShieldDistortionFlow", "shield_distortion_flow", 4132526790u},
    {"CellularRegions", "cellular_regions", 675632435u},
    {"ShieldCells", "shield_cells", 1256329815u},
    {"FractureEdges", "fracture_edges", 3619324494u},
    {"NebulaNormal", "nebula_normal", 3623759407u},
    {"ShieldCellsNormal", "shield_cells_normal", 293241626u},
    {"RingDistance", "ring_distance", 1468568709u},
    {"ShieldCellsDistance", "shield_cells_distance", 2175695932u},
};

class DefaultImageChecksum : public testing::TestWithParam<ChecksumCase> {};

TEST_P(DefaultImageChecksum, Matches) {
    auto const& test{GetParam()};
    auto const requests{default_generation_requests()};
    auto const request{
        std::ranges::find(requests, test.output_name, &GenerationRequest::output_name)};
    ASSERT_NE(request, requests.end());
    EXPECT_EQ(pixel_checksum(generate_image(*request)), test.expected_checksum);
}

INSTANTIATE_TEST_SUITE_P(Examples,
                         DefaultImageChecksum,
                         testing::ValuesIn(checksum_cases),
                         [](auto const& info) { return info.param.name; });

TEST(GenerationRequests, DefaultRequestsAreNamedUniqueAndGenerateValidImages) {
    auto const requests{default_generation_requests()};
    EXPECT_EQ(requests.size(), 18);

    std::unordered_set<std::string> output_names;
    for (auto const& request : requests) {
        EXPECT_FALSE(request.output_name.empty());
        output_names.insert(request.output_name);
        EXPECT_TRUE(generate_image(request).is_valid());
        EXPECT_TRUE(describe_request(request).starts_with("version=6;"));
    }
    EXPECT_EQ(output_names.size(), requests.size());
}

TEST(GenerationRequests, SeedChangesFlowThroughUnifiedRequests) {
    auto first{make_default_request(GeneratorType::Starfield)};
    auto second{first};
    second.starfield.seed += 1;

    EXPECT_PRED_FORMAT2(structurally_equal, generate_image(first), generate_image(first));
    EXPECT_NE(generate_image(first).pixels, generate_image(second).pixels);
}

TEST(GenerationRequests, PostProcessingShapesIntensityAndMaskAlpha) {
    auto request{make_default_request(GeneratorType::RadialGradient)};
    request.radial_gradient = {.width = 65, .height = 65};
    request.post_process.invert = true;
    auto const inverted{generate_image(request)};

    EXPECT_EQ(pixel_at(inverted, 32, 32), transparent);
    EXPECT_EQ(pixel_at(inverted, 0, 0), white);

    request.post_process = {.contrast = 0.0f};
    auto const flat{generate_image(request)};
    EXPECT_EQ(pixel_at(flat, 32, 32), (Pixel{128, 128, 128, 128}));
}

TEST(GenerationRequests, ThresholdSupportsHardAndSoftShaping) {
    auto request{make_default_request(GeneratorType::RadialGradient)};
    request.radial_gradient = {.width = 65, .height = 65};
    for (auto const softness : {0.0f, 0.4f}) {
        SCOPED_TRACE(softness);
        request.post_process = {
            .threshold_enabled = true, .threshold = 0.5f, .threshold_softness = softness};
        auto const image{generate_image(request)};
        EXPECT_EQ(std::ranges::any_of(
                      image.pixels, [](Pixel pixel) { return pixel.red != 0 && pixel.red != 255; }),
                  softness > 0.0f);
    }
}

TEST(RadialAndRingGenerators, RadialGradientHasBrightCentreTransparentEdgeAndSymmetry) {
    auto const image{generate_radial_gradient({.width = 65, .height = 65})};
    auto const center{pixel_at(image, 32, 32)};
    auto const corner{pixel_at(image, 0, 0)};

    EXPECT_EQ(center.red, std::uint8_t{255});
    EXPECT_EQ(center.alpha, std::uint8_t{255});
    EXPECT_EQ(corner.red, std::uint8_t{0});
    EXPECT_EQ(corner.alpha, std::uint8_t{0});

    for (std::int32_t offset{0}; offset <= 32; ++offset) {
        auto const horizontal_left{pixel_at(image, 32 - offset, 32).red};
        auto const horizontal_right{pixel_at(image, 32 + offset, 32).red};
        auto const vertical_top{pixel_at(image, 32, 32 - offset).red};
        auto const vertical_bottom{pixel_at(image, 32, 32 + offset).red};
        EXPECT_EQ(horizontal_left, horizontal_right);
        EXPECT_EQ(vertical_top, vertical_bottom);
        EXPECT_EQ(horizontal_left, vertical_top);
    }
}

TEST(RadialAndRingGenerators, RingMaskPeaksAtConfiguredRadiusAndFallsAway) {
    RingMaskParameters parameters{
        .width = 65, .height = 65, .radius = 0.6f, .thickness = 0.1f, .falloff = 0.04f};
    auto const image{generate_ring_mask(parameters)};
    auto const center_x{32};
    auto const ring_x{center_x + static_cast<std::int32_t>(std::round(parameters.radius * 32.5f))};

    EXPECT_EQ(pixel_at(image, center_x, 32).red, std::uint8_t{0});
    EXPECT_GE(pixel_at(image, ring_x, 32).red, std::uint8_t{240});
    EXPECT_EQ(pixel_at(image, 0, 0).red, std::uint8_t{0});
}

TEST(ShockwaveFlipbookGenerator, FramesExpandFadeAndLeaveUnusedCellsEmpty) {
    ShockwaveFlipbookParameters const parameters{.width = 260,
                                                 .height = 65,
                                                 .columns = 4,
                                                 .rows = 1,
                                                 .frame_count = 3,
                                                 .start_radius = 0.30f,
                                                 .end_radius = 0.75f,
                                                 .thickness = 0.08f,
                                                 .falloff = 0.03f,
                                                 .start_intensity = 1.0f,
                                                 .end_intensity = 0.25f};
    auto const first{generate_shockwave_flipbook(parameters)};
    EXPECT_PRED_FORMAT2(structurally_equal, first, generate_shockwave_flipbook(parameters));

    constexpr std::int32_t frame_width{65};
    std::uint8_t previous_peak{255};
    std::int32_t previous_peak_offset{-1};
    for (std::int32_t frame{0}; frame < parameters.frame_count; ++frame) {
        std::uint8_t peak{0};
        std::int32_t peak_offset{0};
        for (std::int32_t offset{0}; offset <= frame_width / 2; ++offset) {
            auto const value{pixel_at(first, frame * frame_width + 32 + offset, 32).red};
            if (value > peak) {
                peak = value;
                peak_offset = offset;
            }
        }

        EXPECT_GT(peak, 0);
        EXPECT_GT(peak_offset, previous_peak_offset);
        EXPECT_TRUE(frame == 0 || peak < previous_peak);
        previous_peak = peak;
        previous_peak_offset = peak_offset;
    }

    for (std::int32_t y{0}; y < parameters.height; ++y) {
        for (std::int32_t x{3 * frame_width}; x < parameters.width; ++x) {
            EXPECT_EQ(pixel_at(first, x, y), transparent);
        }
    }
}

TEST(SeededGenerators, StarfieldIsDeterministicSparseAndSeeded) {
    StarfieldParameters parameters{.width = 96, .height = 64, .seed = 12345u, .star_count = 40};
    auto const first{
        expect_seeded(generate_starfield, parameters, &StarfieldParameters::seed, 54321u)};

    std::int32_t illuminated_pixel_count{0};
    for (auto const& pixel : first.pixels) {
        illuminated_pixel_count += pixel.red > 0 ? 1 : 0;
        EXPECT_EQ(pixel.red, pixel.alpha);
    }
    EXPECT_GT(illuminated_pixel_count, 0);
    EXPECT_LT(static_cast<std::size_t>(illuminated_pixel_count), first.pixels.size() / 4);
}

TEST(SeededGenerators, NoiseIsDeterministicSeededNonconstantAndSpatiallyCoherent) {
    NoiseParameters parameters{.width = 96,
                               .height = 64,
                               .seed = 9876u,
                               .base_scale = 32.0f,
                               .octave_count = 4,
                               .persistence = 0.5f};
    auto const first{expect_seeded(generate_noise, parameters, &NoiseParameters::seed, 6789u)};

    std::uint8_t minimum_value{255};
    std::uint8_t maximum_value{0};
    std::int64_t neighbor_difference_sum{0};
    std::int32_t neighbor_count{0};
    for (std::int32_t y{0}; y < first.height; ++y) {
        for (std::int32_t x{0}; x < first.width; ++x) {
            auto const value{pixel_at(first, x, y).red};
            minimum_value = std::min(minimum_value, value);
            maximum_value = std::max(maximum_value, value);
            if (x + 1 < first.width) {
                neighbor_difference_sum +=
                    std::abs(static_cast<std::int32_t>(value) - pixel_at(first, x + 1, y).red);
                ++neighbor_count;
            }
        }
    }

    EXPECT_GE(maximum_value - minimum_value, 64);
    auto const average_neighbor_difference{static_cast<float>(neighbor_difference_sum) /
                                           static_cast<float>(neighbor_count)};
    EXPECT_LT(average_neighbor_difference, 20.0f);
}

TEST(SeededGenerators, DefaultSeededOutputsHaveStableChecksums) {
    EXPECT_EQ(pixel_checksum(generate_starfield(StarfieldParameters{})), std::uint32_t{516592973});
    EXPECT_EQ(pixel_checksum(generate_noise(NoiseParameters{})), std::uint32_t{1451590235});
}

TEST(SeededGenerators, TileableNoiseHasMatchingOppositeEdges) {
    auto const image{generate_noise({.width = 97,
                                     .height = 65,
                                     .seed = 13579u,
                                     .base_scale = 24.0f,
                                     .octave_count = 5,
                                     .persistence = 0.55f,
                                     .tileable = true})};
    expect_matching_edges(image);

    EXPECT_NE(pixel_at(image, 0, 0), pixel_at(image, image.width / 2, image.height / 2));
}

TEST(DomainWarpedNoiseGenerator, IsDeterministicSeededNonconstantAndTileable) {
    DomainWarpedNoiseParameters parameters{.width = 97,
                                           .height = 65,
                                           .base_seed = 2468u,
                                           .warp_seed = 1357u,
                                           .base_scale = 28.0f,
                                           .warp_scale = 44.0f,
                                           .warp_strength = 24.0f,
                                           .base_octave_count = 4,
                                           .warp_octave_count = 3,
                                           .persistence = 0.55f,
                                           .tileable = true};
    auto const first{expect_seeded(generate_domain_warped_noise,
                                   parameters,
                                   &DomainWarpedNoiseParameters::warp_seed,
                                   parameters.warp_seed + 1)};
    expect_matching_edges(first);

    auto const values{std::ranges::minmax(first.pixels, {}, &Pixel::red)};
    EXPECT_GE(values.max.red - values.min.red, 64);
}

TEST(CurlNoiseFlowGenerator, IsDeterministicNormalizedAndTileable) {
    CurlNoiseFlowParameters parameters{.width = 97,
                                       .height = 65,
                                       .seed = 8642u,
                                       .base_scale = 36.0f,
                                       .octave_count = 4,
                                       .persistence = 0.55f,
                                       .derivative_step = 1.5f,
                                       .strength = 0.8f,
                                       .tileable = true};
    auto const first{expect_seeded(
        generate_curl_noise_flow, parameters, &CurlNoiseFlowParameters::seed, parameters.seed + 1)};
    expect_matching_edges(first);

    for (auto const pixel : first.pixels) {
        auto const flow_x{static_cast<float>(pixel.red) / 127.5f - 1.0f};
        auto const flow_y{static_cast<float>(pixel.green) / 127.5f - 1.0f};
        auto const magnitude{std::sqrt(flow_x * flow_x + flow_y * flow_y)};
        EXPECT_NEAR(magnitude, 0.8f, 0.02f);
        EXPECT_EQ(pixel.blue, 128);
        EXPECT_EQ(pixel.alpha, 255);
    }
}

TEST(CurlNoiseFlowGenerator, ScalarPostProcessingDoesNotAlterEncodedVectors) {
    auto request{make_default_request(GeneratorType::CurlNoiseFlow)};
    request.curl_noise_flow = {.width = 48, .height = 32};
    request.post_process = {.invert = true,
                            .contrast = 0.0f,
                            .threshold_enabled = true,
                            .threshold = 0.9f,
                            .threshold_softness = 0.0f};

    EXPECT_EQ(generate_image(request).pixels,
              generate_curl_noise_flow(request.curl_noise_flow).pixels);
}

TEST(CellularNoiseGenerator, IsDeterministicSeededAndTileable) {
    CellularNoiseParameters parameters{.width = 97,
                                       .height = 65,
                                       .seed = 97531u,
                                       .cell_size = 18.0f,
                                       .jitter = 0.8f,
                                       .mode = CellularMode::Distance,
                                       .tileable = true};
    auto const first{expect_seeded(
        generate_cellular_noise, parameters, &CellularNoiseParameters::seed, parameters.seed + 1)};
    expect_matching_edges(first);

    auto const values{std::ranges::minmax(first.pixels, {}, &Pixel::red)};
    EXPECT_GE(values.max.red - values.min.red, 64);
    for (auto const pixel : first.pixels) {
        EXPECT_EQ(pixel, (Pixel{pixel.red, pixel.red, pixel.red, 255}));
    }
}

TEST(CellularNoiseGenerator, ZeroJitterProducesRegularCells) {
    auto const image{generate_cellular_noise({.width = 65,
                                              .height = 65,
                                              .seed = 123u,
                                              .cell_size = 16.0f,
                                              .jitter = 0.0f,
                                              .mode = CellularMode::Distance,
                                              .tileable = true})};

    EXPECT_EQ(pixel_at(image, 8, 8), pixel_at(image, 24, 8));
    EXPECT_EQ(pixel_at(image, 8, 8), pixel_at(image, 8, 24));
    EXPECT_GT(pixel_at(image, 16, 8).red, pixel_at(image, 8, 8).red);
}

TEST(CellularNoiseGenerator, BorderModeProducesLinesAndCellInteriors) {
    auto const image{generate_cellular_noise({.width = 97,
                                              .height = 65,
                                              .seed = 24680u,
                                              .cell_size = 20.0f,
                                              .jitter = 1.0f,
                                              .mode = CellularMode::Borders,
                                              .edge_width = 0.75f,
                                              .falloff = 1.25f,
                                              .tileable = true})};

    EXPECT_TRUE(std::ranges::any_of(image.pixels, [](Pixel pixel) { return pixel.red >= 220; }));
    EXPECT_TRUE(std::ranges::any_of(image.pixels, [](Pixel pixel) { return pixel.red <= 20; }));
}

TEST(NormalMapGeneration, FlatHeightProducesNeutralNormal) {
    GeneratedImage height{.width = 8, .height = 6, .pixels = {}, .error = {}};
    height.pixels.assign(static_cast<std::size_t>(height.width * height.height),
                         Pixel{96, 96, 96, 255});

    auto const normal{generate_normal_map(height, 12.0f, false)};

    expect_valid_image(normal, height.width, height.height);
    for (auto const pixel : normal.pixels) {
        EXPECT_EQ(pixel, (Pixel{128, 128, 255, 255}));
    }
}

TEST(NormalMapGeneration, SeededTileableHeightProducesDeterministicSeamlessNormals) {
    auto request{make_default_request(GeneratorType::Noise)};
    request.noise = {.width = 65,
                     .height = 49,
                     .seed = 87654u,
                     .base_scale = 18.0f,
                     .octave_count = 4,
                     .persistence = 0.5f,
                     .tileable = true};
    request.post_process = {.output = ImagePostProcessParameters::Output::NormalMap,
                            .normal_strength = 8.0f,
                            .normal_wrap = true};

    auto const first{generate_image(request)};
    EXPECT_PRED_FORMAT2(structurally_equal, first, generate_image(request));

    expect_matching_edges(first);

    std::int32_t directional_pixel_count{};
    for (auto const pixel : first.pixels) {
        directional_pixel_count += pixel.red != 128 || pixel.green != 128 ? 1 : 0;
        EXPECT_EQ(pixel.alpha, 255);
    }
    EXPECT_GT(static_cast<std::size_t>(directional_pixel_count), first.pixels.size() / 2);
}

TEST(SignedDistanceGeneration, EncodesInsideBoundaryAndOutsideInOrder) {
    GeneratedImage mask{.width = 17, .height = 17, .pixels = {}, .error = {}};
    mask.pixels.assign(static_cast<std::size_t>(mask.width * mask.height), black);
    for (std::int32_t y{6}; y <= 10; ++y) {
        for (std::int32_t x{6}; x <= 10; ++x) {
            mask.pixels[y * mask.width + x] = white;
        }
    }

    auto const distance{generate_signed_distance_field(mask, 0.5f, 6.0f, false)};

    auto const center{pixel_at(distance, 8, 8).red};
    auto const inside_edge{pixel_at(distance, 6, 8).red};
    auto const outside_edge{pixel_at(distance, 5, 8).red};
    auto const far_outside{pixel_at(distance, 0, 8).red};
    EXPECT_GT(center, inside_edge);
    EXPECT_GT(inside_edge, 128);
    EXPECT_LT(outside_edge, 128);
    EXPECT_LT(far_outside, outside_edge);
    EXPECT_EQ(pixel_at(distance, 4, 8), pixel_at(distance, 12, 8));
    EXPECT_EQ(pixel_at(distance, 8, 4), pixel_at(distance, 8, 12));
}

TEST(SignedDistanceGeneration, TileableMasksProduceDeterministicSeamlessDistanceFields) {
    auto request{make_default_request(GeneratorType::CellularNoise)};
    request.cellular_noise = {.width = 65,
                              .height = 49,
                              .seed = 13579u,
                              .cell_size = 14.0f,
                              .jitter = 0.5f,
                              .mode = CellularMode::Borders,
                              .edge_width = 1.0f,
                              .falloff = 1.0f,
                              .tileable = true};
    request.post_process = {.output = ImagePostProcessParameters::Output::SignedDistance,
                            .distance_threshold = 0.5f,
                            .distance_range = 8.0f,
                            .distance_wrap = true};

    auto const first{generate_image(request)};
    EXPECT_PRED_FORMAT2(structurally_equal, first, generate_image(request));

    expect_matching_edges(first);

    for (auto const pixel : first.pixels) {
        EXPECT_EQ(pixel, (Pixel{pixel.red, pixel.red, pixel.red, 255}));
    }
    EXPECT_TRUE(std::ranges::any_of(first.pixels, [](Pixel pixel) { return pixel.red > 128; }));
    EXPECT_TRUE(std::ranges::any_of(first.pixels, [](Pixel pixel) { return pixel.red < 128; }));
}

TEST(HexGridGenerator, ProducesCleanLineAndBackgroundRegions) {
    auto const image{generate_hex_grid({.width = 96,
                                        .height = 64,
                                        .cell_radius = 14.0f,
                                        .line_thickness = 1.5f,
                                        .falloff = 1.0f})};

    for (auto const& pixel : image.pixels) {
        EXPECT_EQ(pixel.red, pixel.alpha);
    }

    EXPECT_TRUE(std::ranges::any_of(image.pixels, [](Pixel pixel) { return pixel.red >= 220; }));
    EXPECT_TRUE(std::ranges::any_of(image.pixels, [](Pixel pixel) { return pixel.red <= 20; }));
}

}
