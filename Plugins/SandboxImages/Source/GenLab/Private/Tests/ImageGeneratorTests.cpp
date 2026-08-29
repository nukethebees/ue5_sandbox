#include "Editor/GenLabSettings.h"
#include "Generation/ImageGenerators.h"

#include <CQTest.h>

namespace SandboxImages::GenLab {
namespace {
auto pixel_at(FGeneratedImage const& image, int32 const x, int32 const y) -> FColor const& {
    return image.pixels[y * image.width + x];
}

void test_valid_image(FAutomationTestBase& test,
                      TCHAR const* const name,
                      FGeneratedImage const& image,
                      int32 const expected_width,
                      int32 const expected_height) {
    test.TestTrue(*FString::Printf(TEXT("%s is valid"), name), image.is_valid());
    test.TestEqual(*FString::Printf(TEXT("%s width"), name), image.width, expected_width);
    test.TestEqual(*FString::Printf(TEXT("%s height"), name), image.height, expected_height);
    test.TestEqual(*FString::Printf(TEXT("%s pixel count"), name),
                   image.pixels.Num(),
                   expected_width * expected_height);
}

auto pixel_checksum(FGeneratedImage const& image) -> uint32 {
    uint32 checksum{2166136261u};
    for (auto const& pixel : image.pixels) {
        for (auto const channel : {pixel.R, pixel.G, pixel.B, pixel.A}) {
            checksum = (checksum ^ channel) * 16777619u;
        }
    }
    return checksum;
}
}

TEST_CLASS(ImageGeneratorBuffers, "SandboxImages.UnitTests")
{
    TEST_METHOD(ProducesCompleteBuffersAtRequestedDimensions)
    {
        constexpr int32 width{48};
        constexpr int32 height{32};

        test_valid_image(*TestRunner,
                         TEXT("radial gradient"),
                         generate_radial_gradient({.width = width, .height = height}),
                         width,
                         height);
        test_valid_image(*TestRunner,
                         TEXT("ring mask"),
                         generate_ring_mask({.width = width, .height = height}),
                         width,
                         height);
        test_valid_image(*TestRunner,
                         TEXT("shockwave flipbook"),
                         generate_shockwave_flipbook({.width = width, .height = height}),
                         width,
                         height);
        test_valid_image(*TestRunner,
                         TEXT("starfield"),
                         generate_starfield({.width = width, .height = height}),
                         width,
                         height);
        test_valid_image(*TestRunner,
                         TEXT("noise"),
                         generate_noise({.width = width, .height = height}),
                         width,
                         height);
        test_valid_image(*TestRunner,
                         TEXT("domain-warped noise"),
                         generate_domain_warped_noise({.width = width, .height = height}),
                         width,
                         height);
        test_valid_image(*TestRunner,
                         TEXT("curl-noise flow"),
                         generate_curl_noise_flow({.width = width, .height = height}),
                         width,
                         height);
        test_valid_image(*TestRunner,
                         TEXT("cellular noise"),
                         generate_cellular_noise({.width = width, .height = height}),
                         width,
                         height);
        test_valid_image(*TestRunner,
                         TEXT("hex grid"),
                         generate_hex_grid({.width = width, .height = height}),
                         width,
                         height);
        auto const mask{generate_ring_mask({.width = width, .height = height})};
        test_valid_image(*TestRunner,
                         TEXT("signed distance"),
                         generate_signed_distance_field(mask, 0.5f, 8.0f, false),
                         width,
                         height);
    }

    TEST_METHOD(RejectsInvalidParametersWithUsefulErrors)
    {
        auto const invalid_dimensions{generate_radial_gradient({.width = 0})};
        auto const invalid_ring{generate_ring_mask({.thickness = 0.0f})};
        auto const invalid_flipbook{generate_shockwave_flipbook({.width = 255})};
        auto const invalid_starfield{generate_starfield({.minimum_radius = -1.0f})};
        auto const invalid_noise{generate_noise({.base_scale = 0.0f})};
        auto const invalid_domain_noise{generate_domain_warped_noise({.warp_octave_count = 0})};
        auto const invalid_flow{generate_curl_noise_flow({.derivative_step = 0.0f})};
        auto const invalid_cellular{generate_cellular_noise({.jitter = 2.0f})};
        auto const invalid_hex{generate_hex_grid({.cell_radius = 0.0f})};
        auto invalid_normal_request{make_default_request(EGeneratorType::Noise)};
        invalid_normal_request.post_process = {
            .output = FImagePostProcessParameters::EOutput::NormalMap, .normal_strength = -1.0f};
        auto const invalid_normal{generate_image(invalid_normal_request)};
        auto invalid_distance_request{make_default_request(EGeneratorType::RingMask)};
        invalid_distance_request.post_process = {
            .output = FImagePostProcessParameters::EOutput::SignedDistance, .distance_range = 0.0f};
        auto const invalid_distance{generate_image(invalid_distance_request)};

        for (auto const* image : {&invalid_dimensions,
                                  &invalid_ring,
                                  &invalid_flipbook,
                                  &invalid_starfield,
                                  &invalid_noise,
                                  &invalid_domain_noise,
                                  &invalid_flow,
                                  &invalid_cellular,
                                  &invalid_hex,
                                  &invalid_normal,
                                  &invalid_distance}) {
            TestRunner->TestFalse(TEXT("Invalid parameters produce no valid image"),
                                  image->is_valid());
            TestRunner->TestFalse(TEXT("Invalid parameters explain the failure"),
                                  image->error.IsEmpty());
        }
    }
};

TEST_CLASS(GenerationRequests, "SandboxImages.UnitTests")
{
    TEST_METHOD(DefaultRequestsAreNamedUniqueAndGenerateValidImages)
    {
        auto const requests{default_generation_requests()};
        TestRunner->TestEqual(TEXT("There are eighteen canonical examples"), requests.Num(), 18);

        TSet<FString> output_names;
        for (auto const& request : requests) {
            TestRunner->TestFalse(TEXT("Default output name is present"),
                                  request.output_name.IsEmpty());
            output_names.Add(request.output_name);
            TestRunner->TestTrue(TEXT("Default request generates a valid image"),
                                 generate_image(request).is_valid());
            TestRunner->TestTrue(TEXT("Request description contains its format version"),
                                 describe_request(request).StartsWith(TEXT("version=6;")));
        }
        TestRunner->TestEqual(
            TEXT("Default output names are unique"), output_names.Num(), requests.Num());
    }

    TEST_METHOD(SeedChangesFlowThroughUnifiedRequests)
    {
        auto first{make_default_request(EGeneratorType::Starfield)};
        auto second{first};
        second.starfield.seed += 1;

        TestRunner->TestTrue(TEXT("Same request remains deterministic"),
                             generate_image(first).pixels == generate_image(first).pixels);
        TestRunner->TestTrue(TEXT("A changed request seed changes output"),
                             generate_image(first).pixels != generate_image(second).pixels);
    }

    TEST_METHOD(PostProcessingShapesIntensityAndMaskAlpha)
    {
        auto request{make_default_request(EGeneratorType::RadialGradient)};
        request.radial_gradient = {.width = 65, .height = 65};
        request.post_process.invert = true;
        auto const inverted{generate_image(request)};

        TestRunner->TestEqual(TEXT("Invert darkens the original bright centre"),
                              pixel_at(inverted, 32, 32),
                              FColor::Transparent);
        TestRunner->TestEqual(TEXT("Invert brightens the original transparent corner"),
                              pixel_at(inverted, 0, 0),
                              FColor::White);

        request.post_process = {.contrast = 0.0f};
        auto const flat{generate_image(request)};
        TestRunner->TestEqual(TEXT("Zero contrast produces middle gray mask intensity"),
                              pixel_at(flat, 32, 32),
                              FColor{128, 128, 128, 128});

        request.post_process.contrast = -1.0f;
        auto const invalid{generate_image(request)};
        TestRunner->TestFalse(TEXT("Negative contrast is rejected"), invalid.is_valid());
        TestRunner->TestFalse(TEXT("Invalid contrast explains the failure"),
                              invalid.error.IsEmpty());
    }

    TEST_METHOD(ThresholdSupportsHardAndSoftShaping)
    {
        auto request{make_default_request(EGeneratorType::RadialGradient)};
        request.radial_gradient = {.width = 65, .height = 65};
        request.post_process = {
            .threshold_enabled = true, .threshold = 0.5f, .threshold_softness = 0.0f};
        auto const hard{generate_image(request)};

        int32 hard_intermediate_count{0};
        for (auto const pixel : hard.pixels) {
            hard_intermediate_count += pixel.R != 0 && pixel.R != 255 ? 1 : 0;
        }
        TestRunner->TestEqual(
            TEXT("Hard threshold produces binary intensity"), hard_intermediate_count, 0);

        request.post_process.threshold_softness = 0.4f;
        auto const soft{generate_image(request)};
        int32 soft_intermediate_count{0};
        for (auto const pixel : soft.pixels) {
            soft_intermediate_count += pixel.R != 0 && pixel.R != 255 ? 1 : 0;
        }
        TestRunner->TestTrue(TEXT("Soft threshold preserves a transition band"),
                             soft_intermediate_count > 0);
    }
};

TEST_CLASS(GenLabPresetSettings, "SandboxImages.UnitTests")
{
    TEST_METHOD(CanonicalRequestsRoundTripThroughEditorSettings)
    {
        auto* const settings{NewObject<UGenLabSettings>()};
        TestRunner->TestNotNull(TEXT("Transient GenLab settings are created"), settings);
        if (settings == nullptr) {
            return;
        }

        for (auto const& expected : default_generation_requests()) {
            settings->load_request(expected);
            auto const actual{settings->to_request()};
            TestRunner->TestEqual(*FString::Printf(TEXT("%s output name"), *expected.output_name),
                                  actual.output_name,
                                  expected.output_name);
            TestRunner->TestEqual(*FString::Printf(TEXT("%s parameters"), *expected.output_name),
                                  describe_request(actual),
                                  describe_request(expected));
        }
    }

    TEST_METHOD(ChangingGeneratorPreservesSharedOutputShaping)
    {
        auto* const settings{NewObject<UGenLabSettings>()};
        settings->generator = EGenLabGenerator::CellularNoise;
        settings->invert = true;
        settings->contrast = 1.75f;
        settings->threshold_enabled = true;
        settings->threshold = 0.3f;
        settings->threshold_softness = 0.2f;
        settings->output = EGenLabOutput::NormalMap;
        settings->normal_strength = 5.0f;
        settings->normal_wrap = true;

        settings->load_generator_defaults();

        auto const request{settings->to_request()};
        TestRunner->TestEqual(TEXT("Generator defaults select cellular noise"),
                              request.generator,
                              EGeneratorType::CellularNoise);
        TestRunner->TestTrue(TEXT("Invert is preserved"), request.post_process.invert);
        TestRunner->TestEqual(TEXT("Contrast is preserved"), request.post_process.contrast, 1.75f);
        TestRunner->TestTrue(TEXT("Threshold is preserved"),
                             request.post_process.threshold_enabled);
        TestRunner->TestEqual(
            TEXT("Threshold value is preserved"), request.post_process.threshold, 0.3f);
        TestRunner->TestEqual(
            TEXT("Threshold softness is preserved"), request.post_process.threshold_softness, 0.2f);
        TestRunner->TestEqual(TEXT("Output encoding is preserved"),
                              request.post_process.output,
                              FImagePostProcessParameters::EOutput::NormalMap);
        TestRunner->TestEqual(
            TEXT("Normal strength is preserved"), request.post_process.normal_strength, 5.0f);
        TestRunner->TestTrue(TEXT("Normal wrapping is preserved"),
                             request.post_process.normal_wrap);
    }
};

TEST_CLASS(RadialAndRingGenerators, "SandboxImages.UnitTests")
{
    TEST_METHOD(RadialGradientHasBrightCentreTransparentEdgeAndSymmetry)
    {
        auto const image{generate_radial_gradient({.width = 65, .height = 65})};
        auto const center{pixel_at(image, 32, 32)};
        auto const corner{pixel_at(image, 0, 0)};

        TestRunner->TestEqual(TEXT("Centre is white"), center.R, uint8{255});
        TestRunner->TestEqual(TEXT("Centre is opaque"), center.A, uint8{255});
        TestRunner->TestEqual(TEXT("Corner is black"), corner.R, uint8{0});
        TestRunner->TestEqual(TEXT("Corner is transparent"), corner.A, uint8{0});

        for (int32 offset{0}; offset <= 32; ++offset) {
            auto const horizontal_left{pixel_at(image, 32 - offset, 32).R};
            auto const horizontal_right{pixel_at(image, 32 + offset, 32).R};
            auto const vertical_top{pixel_at(image, 32, 32 - offset).R};
            auto const vertical_bottom{pixel_at(image, 32, 32 + offset).R};
            TestRunner->TestEqual(
                TEXT("Horizontal samples are symmetric"), horizontal_left, horizontal_right);
            TestRunner->TestEqual(
                TEXT("Vertical samples are symmetric"), vertical_top, vertical_bottom);
            TestRunner->TestEqual(TEXT("Axes have equal falloff"), horizontal_left, vertical_top);
        }
    }

    TEST_METHOD(RingMaskPeaksAtConfiguredRadiusAndFallsAway)
    {
        FRingMaskParameters parameters{
            .width = 65, .height = 65, .radius = 0.6f, .thickness = 0.1f, .falloff = 0.04f};
        auto const image{generate_ring_mask(parameters)};
        auto const center_x{32};
        auto const ring_x{center_x + FMath::RoundToInt(parameters.radius * 32.5f)};

        TestRunner->TestEqual(
            TEXT("Image centre is outside the ring"), pixel_at(image, center_x, 32).R, uint8{0});
        TestRunner->TestTrue(TEXT("Configured radius lies on the bright ring"),
                             pixel_at(image, ring_x, 32).R >= uint8{240});
        TestRunner->TestEqual(
            TEXT("Image corner is outside the ring"), pixel_at(image, 0, 0).R, uint8{0});
    }
};

TEST_CLASS(ShockwaveFlipbookGenerator, "SandboxImages.UnitTests")
{
    TEST_METHOD(FramesExpandFadeAndLeaveUnusedCellsEmpty)
    {
        FShockwaveFlipbookParameters const parameters{.width = 260,
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
        auto const second{generate_shockwave_flipbook(parameters)};
        TestRunner->TestTrue(TEXT("Identical parameters produce identical flipbooks"),
                             first.pixels == second.pixels);

        constexpr int32 frame_width{65};
        uint8 previous_peak{255};
        int32 previous_peak_offset{-1};
        for (int32 frame{0}; frame < parameters.frame_count; ++frame) {
            uint8 peak{0};
            int32 peak_offset{0};
            for (int32 offset{0}; offset <= frame_width / 2; ++offset) {
                auto const value{pixel_at(first, frame * frame_width + 32 + offset, 32).R};
                if (value > peak) {
                    peak = value;
                    peak_offset = offset;
                }
            }

            TestRunner->TestTrue(TEXT("Each frame contains a visible ring"), peak > 0);
            TestRunner->TestTrue(TEXT("The ring expands from frame to frame"),
                                 peak_offset > previous_peak_offset);
            TestRunner->TestTrue(TEXT("The ring fades from frame to frame"),
                                 frame == 0 || peak < previous_peak);
            previous_peak = peak;
            previous_peak_offset = peak_offset;
        }

        int32 unused_nonzero_pixels{0};
        for (int32 y{0}; y < parameters.height; ++y) {
            for (int32 x{3 * frame_width}; x < parameters.width; ++x) {
                unused_nonzero_pixels += pixel_at(first, x, y) != FColor::Transparent ? 1 : 0;
            }
        }
        TestRunner->TestEqual(
            TEXT("Unused flipbook cells stay transparent"), unused_nonzero_pixels, 0);
    }
};

TEST_CLASS(SeededGenerators, "SandboxImages.UnitTests")
{
    TEST_METHOD(StarfieldIsDeterministicSparseAndSeeded)
    {
        FStarfieldParameters parameters{
            .width = 96, .height = 64, .seed = 12345u, .star_count = 40};
        auto const first{generate_starfield(parameters)};
        auto const second{generate_starfield(parameters)};
        parameters.seed = 54321u;
        auto const different_seed{generate_starfield(parameters)};

        TestRunner->TestTrue(TEXT("Same seed produces identical pixels"),
                             first.pixels == second.pixels);
        TestRunner->TestTrue(TEXT("Different seed changes the starfield"),
                             first.pixels != different_seed.pixels);

        int32 illuminated_pixel_count{0};
        int32 channel_mismatch_count{0};
        for (auto const& pixel : first.pixels) {
            illuminated_pixel_count += pixel.R > 0 ? 1 : 0;
            channel_mismatch_count += pixel.R != pixel.A ? 1 : 0;
        }
        TestRunner->TestTrue(TEXT("Starfield contains stars"), illuminated_pixel_count > 0);
        TestRunner->TestTrue(TEXT("Starfield remains sparse"),
                             illuminated_pixel_count < first.pixels.Num() / 4);
        TestRunner->TestEqual(
            TEXT("Transparent starfield RGB and alpha form one mask"), channel_mismatch_count, 0);
    }

    TEST_METHOD(NoiseIsDeterministicSeededNonconstantAndSpatiallyCoherent)
    {
        FNoiseParameters parameters{.width = 96,
                                    .height = 64,
                                    .seed = 9876u,
                                    .base_scale = 32.0f,
                                    .octave_count = 4,
                                    .persistence = 0.5f};
        auto const first{generate_noise(parameters)};
        auto const second{generate_noise(parameters)};
        parameters.seed = 6789u;
        auto const different_seed{generate_noise(parameters)};

        TestRunner->TestTrue(TEXT("Same seed produces identical noise"),
                             first.pixels == second.pixels);
        TestRunner->TestTrue(TEXT("Different seed changes the noise"),
                             first.pixels != different_seed.pixels);

        uint8 minimum_value{255};
        uint8 maximum_value{0};
        int64 neighbor_difference_sum{0};
        int32 neighbor_count{0};
        for (int32 y{0}; y < first.height; ++y) {
            for (int32 x{0}; x < first.width; ++x) {
                auto const value{pixel_at(first, x, y).R};
                minimum_value = FMath::Min(minimum_value, value);
                maximum_value = FMath::Max(maximum_value, value);
                if (x + 1 < first.width) {
                    neighbor_difference_sum +=
                        FMath::Abs(static_cast<int32>(value) - pixel_at(first, x + 1, y).R);
                    ++neighbor_count;
                }
            }
        }

        TestRunner->TestTrue(TEXT("Noise spans a useful value range"),
                             maximum_value - minimum_value >= 64);
        auto const average_neighbor_difference{static_cast<float>(neighbor_difference_sum) /
                                               static_cast<float>(neighbor_count)};
        TestRunner->TestTrue(TEXT("Adjacent noise pixels vary smoothly"),
                             average_neighbor_difference < 20.0f);
    }

    TEST_METHOD(DefaultSeededOutputsHaveStableChecksums)
    {
        TestRunner->TestEqual(TEXT("Default starfield checksum"),
                              pixel_checksum(generate_starfield(FStarfieldParameters{})),
                              uint32{516592973});
        TestRunner->TestEqual(TEXT("Default noise checksum"),
                              pixel_checksum(generate_noise(FNoiseParameters{})),
                              uint32{1451590235});
    }

    TEST_METHOD(TileableNoiseHasMatchingOppositeEdges)
    {
        auto const image{generate_noise({.width = 97,
                                         .height = 65,
                                         .seed = 13579u,
                                         .base_scale = 24.0f,
                                         .octave_count = 5,
                                         .persistence = 0.55f,
                                         .tileable = true})};
        TestRunner->TestTrue(TEXT("Tileable noise is valid"), image.is_valid());

        for (int32 y{0}; y < image.height; ++y) {
            TestRunner->TestEqual(TEXT("Left and right edges match"),
                                  pixel_at(image, 0, y),
                                  pixel_at(image, image.width - 1, y));
        }
        for (int32 x{0}; x < image.width; ++x) {
            TestRunner->TestEqual(TEXT("Top and bottom edges match"),
                                  pixel_at(image, x, 0),
                                  pixel_at(image, x, image.height - 1));
        }

        TestRunner->TestTrue(TEXT("Tileable noise remains nonconstant"),
                             pixel_at(image, 0, 0) !=
                                 pixel_at(image, image.width / 2, image.height / 2));
    }
};

TEST_CLASS(DomainWarpedNoiseGenerator, "SandboxImages.UnitTests")
{
    TEST_METHOD(IsDeterministicSeededNonconstantAndTileable)
    {
        FDomainWarpedNoiseParameters parameters{.width = 97,
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
        auto const first{generate_domain_warped_noise(parameters)};
        auto const second{generate_domain_warped_noise(parameters)};
        parameters.warp_seed += 1;
        auto const changed_warp_seed{generate_domain_warped_noise(parameters)};

        TestRunner->TestTrue(TEXT("Same parameters produce identical warped noise"),
                             first.pixels == second.pixels);
        TestRunner->TestTrue(TEXT("Warp seed affects the output"),
                             first.pixels != changed_warp_seed.pixels);
        for (int32 y{0}; y < first.height; ++y) {
            TestRunner->TestEqual(TEXT("Warped noise left and right edges match"),
                                  pixel_at(first, 0, y),
                                  pixel_at(first, first.width - 1, y));
        }
        for (int32 x{0}; x < first.width; ++x) {
            TestRunner->TestEqual(TEXT("Warped noise top and bottom edges match"),
                                  pixel_at(first, x, 0),
                                  pixel_at(first, x, first.height - 1));
        }

        uint8 minimum_value{255};
        uint8 maximum_value{0};
        for (auto const pixel : first.pixels) {
            minimum_value = FMath::Min(minimum_value, pixel.R);
            maximum_value = FMath::Max(maximum_value, pixel.R);
        }
        TestRunner->TestTrue(TEXT("Warped noise spans a useful value range"),
                             maximum_value - minimum_value >= 64);
    }

    TEST_METHOD(DefaultExamplesHaveStableChecksums)
    {
        auto const requests{default_generation_requests()};
        for (auto const& request : requests) {
            if (request.generator != EGeneratorType::DomainWarpedNoise ||
                request.post_process.output != FImagePostProcessParameters::EOutput::Scalar) {
                continue;
            }

            uint32 expected_checksum{};
            if (request.output_name == TEXT("nebula_soft")) {
                expected_checksum = 1376257265u;
            } else if (request.output_name == TEXT("energy_filaments")) {
                expected_checksum = 2606448723u;
            } else if (request.output_name == TEXT("shield_turbulence")) {
                expected_checksum = 3121887609u;
            }
            TestRunner->TestEqual(*FString::Printf(TEXT("%s checksum"), *request.output_name),
                                  pixel_checksum(generate_image(request)),
                                  expected_checksum);
        }
    }
};

TEST_CLASS(CurlNoiseFlowGenerator, "SandboxImages.UnitTests")
{
    TEST_METHOD(IsDeterministicNormalizedAndTileable)
    {
        FCurlNoiseFlowParameters parameters{.width = 97,
                                            .height = 65,
                                            .seed = 8642u,
                                            .base_scale = 36.0f,
                                            .octave_count = 4,
                                            .persistence = 0.55f,
                                            .derivative_step = 1.5f,
                                            .strength = 0.8f,
                                            .tileable = true};
        auto const first{generate_curl_noise_flow(parameters)};
        auto const second{generate_curl_noise_flow(parameters)};
        parameters.seed += 1;
        auto const changed_seed{generate_curl_noise_flow(parameters)};

        TestRunner->TestTrue(TEXT("Same flow parameters produce identical output"),
                             first.pixels == second.pixels);
        TestRunner->TestTrue(TEXT("Flow seed affects the output"),
                             first.pixels != changed_seed.pixels);
        for (int32 y{0}; y < first.height; ++y) {
            TestRunner->TestEqual(TEXT("Flow left and right edges match"),
                                  pixel_at(first, 0, y),
                                  pixel_at(first, first.width - 1, y));
        }
        for (int32 x{0}; x < first.width; ++x) {
            TestRunner->TestEqual(TEXT("Flow top and bottom edges match"),
                                  pixel_at(first, x, 0),
                                  pixel_at(first, x, first.height - 1));
        }

        int32 normalized_pixel_count{0};
        int32 neutral_blue_count{0};
        int32 opaque_alpha_count{0};
        for (auto const pixel : first.pixels) {
            auto const flow_x{static_cast<float>(pixel.R) / 127.5f - 1.0f};
            auto const flow_y{static_cast<float>(pixel.G) / 127.5f - 1.0f};
            auto const magnitude{FMath::Sqrt(flow_x * flow_x + flow_y * flow_y)};
            normalized_pixel_count += FMath::IsNearlyEqual(magnitude, 0.8f, 0.02f) ? 1 : 0;
            neutral_blue_count += pixel.B == 128 ? 1 : 0;
            opaque_alpha_count += pixel.A == 255 ? 1 : 0;
        }
        TestRunner->TestEqual(TEXT("Every flow vector has the configured magnitude"),
                              normalized_pixel_count,
                              first.pixels.Num());
        TestRunner->TestEqual(TEXT("Every flow pixel has a neutral blue channel"),
                              neutral_blue_count,
                              first.pixels.Num());
        TestRunner->TestEqual(
            TEXT("Every flow pixel has opaque alpha"), opaque_alpha_count, first.pixels.Num());
    }

    TEST_METHOD(ScalarPostProcessingDoesNotAlterEncodedVectors)
    {
        auto request{make_default_request(EGeneratorType::CurlNoiseFlow)};
        request.curl_noise_flow = {.width = 48, .height = 32};
        request.post_process = {.invert = true,
                                .contrast = 0.0f,
                                .threshold_enabled = true,
                                .threshold = 0.9f,
                                .threshold_softness = 0.0f};

        TestRunner->TestTrue(TEXT("Scalar shaping is bypassed for flow-map data"),
                             generate_image(request).pixels ==
                                 generate_curl_noise_flow(request.curl_noise_flow).pixels);
    }

    TEST_METHOD(DefaultExamplesHaveStableChecksums)
    {
        for (auto const& request : default_generation_requests()) {
            if (request.generator != EGeneratorType::CurlNoiseFlow) {
                continue;
            }

            auto const expected_checksum{request.output_name == TEXT("nebula_flow")
                                             ? uint32{3933359053}
                                             : uint32{4132526790}};
            TestRunner->TestEqual(*FString::Printf(TEXT("%s checksum"), *request.output_name),
                                  pixel_checksum(generate_image(request)),
                                  expected_checksum);
        }
    }
};

TEST_CLASS(CellularNoiseGenerator, "SandboxImages.UnitTests")
{
    TEST_METHOD(IsDeterministicSeededAndTileable)
    {
        FCellularNoiseParameters parameters{.width = 97,
                                            .height = 65,
                                            .seed = 97531u,
                                            .cell_size = 18.0f,
                                            .jitter = 0.8f,
                                            .mode = ECellularMode::Distance,
                                            .tileable = true};
        auto const first{generate_cellular_noise(parameters)};
        auto const second{generate_cellular_noise(parameters)};
        parameters.seed += 1;
        auto const changed_seed{generate_cellular_noise(parameters)};

        TestRunner->TestTrue(TEXT("Same cellular parameters produce identical output"),
                             first.pixels == second.pixels);
        TestRunner->TestTrue(TEXT("Cellular seed affects the output"),
                             first.pixels != changed_seed.pixels);
        for (int32 y{0}; y < first.height; ++y) {
            TestRunner->TestEqual(TEXT("Cellular left and right edges match"),
                                  pixel_at(first, 0, y),
                                  pixel_at(first, first.width - 1, y));
        }
        for (int32 x{0}; x < first.width; ++x) {
            TestRunner->TestEqual(TEXT("Cellular top and bottom edges match"),
                                  pixel_at(first, x, 0),
                                  pixel_at(first, x, first.height - 1));
        }

        uint8 minimum_value{255};
        uint8 maximum_value{0};
        int32 invalid_channel_count{0};
        for (auto const pixel : first.pixels) {
            minimum_value = FMath::Min(minimum_value, pixel.R);
            maximum_value = FMath::Max(maximum_value, pixel.R);
            invalid_channel_count +=
                pixel.R != pixel.G || pixel.R != pixel.B || pixel.A != 255 ? 1 : 0;
        }
        TestRunner->TestTrue(TEXT("Cellular distance spans a useful value range"),
                             maximum_value - minimum_value >= 64);
        TestRunner->TestEqual(
            TEXT("Cellular output is opaque grayscale"), invalid_channel_count, 0);
    }

    TEST_METHOD(ZeroJitterProducesRegularCells)
    {
        auto const image{generate_cellular_noise({.width = 65,
                                                  .height = 65,
                                                  .seed = 123u,
                                                  .cell_size = 16.0f,
                                                  .jitter = 0.0f,
                                                  .mode = ECellularMode::Distance,
                                                  .tileable = true})};

        TestRunner->TestEqual(TEXT("Regular cell centres repeat horizontally"),
                              pixel_at(image, 8, 8),
                              pixel_at(image, 24, 8));
        TestRunner->TestEqual(TEXT("Regular cell centres repeat vertically"),
                              pixel_at(image, 8, 8),
                              pixel_at(image, 8, 24));
        TestRunner->TestTrue(TEXT("Regular cell boundary is farther from a site than its centre"),
                             pixel_at(image, 16, 8).R > pixel_at(image, 8, 8).R);
    }

    TEST_METHOD(BorderModeProducesLinesAndCellInteriors)
    {
        auto const image{generate_cellular_noise({.width = 97,
                                                  .height = 65,
                                                  .seed = 24680u,
                                                  .cell_size = 20.0f,
                                                  .jitter = 1.0f,
                                                  .mode = ECellularMode::Borders,
                                                  .edge_width = 0.75f,
                                                  .falloff = 1.25f,
                                                  .tileable = true})};

        int32 bright_pixel_count{0};
        int32 dark_pixel_count{0};
        for (auto const pixel : image.pixels) {
            bright_pixel_count += pixel.R >= 220 ? 1 : 0;
            dark_pixel_count += pixel.R <= 20 ? 1 : 0;
        }
        TestRunner->TestTrue(TEXT("Cellular borders contain bright lines"), bright_pixel_count > 0);
        TestRunner->TestTrue(TEXT("Cellular borders contain dark interiors"), dark_pixel_count > 0);
    }

    TEST_METHOD(DefaultExamplesHaveStableChecksums)
    {
        for (auto const& request : default_generation_requests()) {
            if (request.generator != EGeneratorType::CellularNoise ||
                request.post_process.output != FImagePostProcessParameters::EOutput::Scalar) {
                continue;
            }

            uint32 expected_checksum{};
            if (request.output_name == TEXT("cellular_regions")) {
                expected_checksum = 675632435u;
            } else if (request.output_name == TEXT("shield_cells")) {
                expected_checksum = 1256329815u;
            } else if (request.output_name == TEXT("fracture_edges")) {
                expected_checksum = 3619324494u;
            }
            TestRunner->TestEqual(*FString::Printf(TEXT("%s checksum"), *request.output_name),
                                  pixel_checksum(generate_image(request)),
                                  expected_checksum);
        }
    }
};

TEST_CLASS(NormalMapGeneration, "SandboxImages.UnitTests")
{
    TEST_METHOD(FlatHeightProducesNeutralNormal)
    {
        FGeneratedImage height{.width = 8, .height = 6};
        height.pixels.Init(FColor{96, 96, 96, 255}, height.width * height.height);

        auto const normal{generate_normal_map(height, 12.0f, false)};

        test_valid_image(*TestRunner, TEXT("flat normal map"), normal, height.width, height.height);
        for (auto const pixel : normal.pixels) {
            TestRunner->TestEqual(TEXT("Flat height has a neutral tangent normal"),
                                  pixel,
                                  FColor{128, 128, 255, 255});
        }
    }

    TEST_METHOD(SeededTileableHeightProducesDeterministicSeamlessNormals)
    {
        auto request{make_default_request(EGeneratorType::Noise)};
        request.noise = {.width = 65,
                         .height = 49,
                         .seed = 87654u,
                         .base_scale = 18.0f,
                         .octave_count = 4,
                         .persistence = 0.5f,
                         .tileable = true};
        request.post_process = {.output = FImagePostProcessParameters::EOutput::NormalMap,
                                .normal_strength = 8.0f,
                                .normal_wrap = true};

        auto const first{generate_image(request)};
        auto const second{generate_image(request)};

        TestRunner->TestTrue(TEXT("Normal-map generation is deterministic"),
                             first.pixels == second.pixels);
        for (int32 y{0}; y < first.height; ++y) {
            TestRunner->TestEqual(TEXT("Normal-map left and right edges match"),
                                  pixel_at(first, 0, y),
                                  pixel_at(first, first.width - 1, y));
        }
        for (int32 x{0}; x < first.width; ++x) {
            TestRunner->TestEqual(TEXT("Normal-map top and bottom edges match"),
                                  pixel_at(first, x, 0),
                                  pixel_at(first, x, first.height - 1));
        }

        int32 directional_pixel_count{};
        int32 invalid_alpha_count{};
        for (auto const pixel : first.pixels) {
            directional_pixel_count += pixel.R != 128 || pixel.G != 128 ? 1 : 0;
            invalid_alpha_count += pixel.A != 255 ? 1 : 0;
        }
        TestRunner->TestTrue(TEXT("Noise produces directional normal variation"),
                             directional_pixel_count > first.pixels.Num() / 2);
        TestRunner->TestEqual(TEXT("Normal maps are opaque"), invalid_alpha_count, 0);
    }

    TEST_METHOD(DefaultExamplesHaveStableChecksums)
    {
        for (auto const& request : default_generation_requests()) {
            if (request.post_process.output != FImagePostProcessParameters::EOutput::NormalMap) {
                continue;
            }

            uint32 expected_checksum{};
            if (request.output_name == TEXT("nebula_normal")) {
                expected_checksum = 3623759407u;
            } else if (request.output_name == TEXT("shield_cells_normal")) {
                expected_checksum = 293241626u;
            }
            TestRunner->TestEqual(*FString::Printf(TEXT("%s checksum"), *request.output_name),
                                  pixel_checksum(generate_image(request)),
                                  expected_checksum);
        }
    }
};

TEST_CLASS(SignedDistanceGeneration, "SandboxImages.UnitTests")
{
    TEST_METHOD(EncodesInsideBoundaryAndOutsideInOrder)
    {
        FGeneratedImage mask{.width = 17, .height = 17};
        mask.pixels.Init(FColor::Black, mask.width * mask.height);
        for (int32 y{6}; y <= 10; ++y) {
            for (int32 x{6}; x <= 10; ++x) {
                mask.pixels[y * mask.width + x] = FColor::White;
            }
        }

        auto const distance{generate_signed_distance_field(mask, 0.5f, 6.0f, false)};

        auto const center{pixel_at(distance, 8, 8).R};
        auto const inside_edge{pixel_at(distance, 6, 8).R};
        auto const outside_edge{pixel_at(distance, 5, 8).R};
        auto const far_outside{pixel_at(distance, 0, 8).R};
        TestRunner->TestTrue(TEXT("Shape centre is farther inside than its inner edge"),
                             center > inside_edge);
        TestRunner->TestTrue(TEXT("Inner edge is encoded above the midpoint"), inside_edge > 128);
        TestRunner->TestTrue(TEXT("Outer edge is encoded below the midpoint"), outside_edge < 128);
        TestRunner->TestTrue(TEXT("Far outside is darker than the outer edge"),
                             far_outside < outside_edge);
        TestRunner->TestEqual(TEXT("Horizontal distance is symmetric"),
                              pixel_at(distance, 4, 8),
                              pixel_at(distance, 12, 8));
        TestRunner->TestEqual(TEXT("Vertical distance is symmetric"),
                              pixel_at(distance, 8, 4),
                              pixel_at(distance, 8, 12));
    }

    TEST_METHOD(TileableMasksProduceDeterministicSeamlessDistanceFields)
    {
        auto request{make_default_request(EGeneratorType::CellularNoise)};
        request.cellular_noise = {.width = 65,
                                  .height = 49,
                                  .seed = 13579u,
                                  .cell_size = 14.0f,
                                  .jitter = 0.5f,
                                  .mode = ECellularMode::Borders,
                                  .edge_width = 1.0f,
                                  .falloff = 1.0f,
                                  .tileable = true};
        request.post_process = {.output = FImagePostProcessParameters::EOutput::SignedDistance,
                                .distance_threshold = 0.5f,
                                .distance_range = 8.0f,
                                .distance_wrap = true};

        auto const first{generate_image(request)};
        auto const second{generate_image(request)};

        TestRunner->TestTrue(TEXT("Signed-distance generation is deterministic"),
                             first.pixels == second.pixels);
        for (int32 y{0}; y < first.height; ++y) {
            TestRunner->TestEqual(TEXT("Distance-field left and right edges match"),
                                  pixel_at(first, 0, y),
                                  pixel_at(first, first.width - 1, y));
        }
        for (int32 x{0}; x < first.width; ++x) {
            TestRunner->TestEqual(TEXT("Distance-field top and bottom edges match"),
                                  pixel_at(first, x, 0),
                                  pixel_at(first, x, first.height - 1));
        }

        int32 inside_pixel_count{};
        int32 outside_pixel_count{};
        int32 invalid_channel_count{};
        for (auto const pixel : first.pixels) {
            inside_pixel_count += pixel.R > 128 ? 1 : 0;
            outside_pixel_count += pixel.R < 128 ? 1 : 0;
            invalid_channel_count +=
                pixel.R != pixel.G || pixel.R != pixel.B || pixel.A != 255 ? 1 : 0;
        }
        TestRunner->TestTrue(TEXT("Distance field contains inside values"), inside_pixel_count > 0);
        TestRunner->TestTrue(TEXT("Distance field contains outside values"),
                             outside_pixel_count > 0);
        TestRunner->TestEqual(TEXT("Distance field is opaque grayscale"), invalid_channel_count, 0);
    }

    TEST_METHOD(DefaultExamplesHaveStableChecksums)
    {
        for (auto const& request : default_generation_requests()) {
            if (request.post_process.output !=
                FImagePostProcessParameters::EOutput::SignedDistance) {
                continue;
            }

            uint32 expected_checksum{};
            if (request.output_name == TEXT("ring_distance")) {
                expected_checksum = 1468568709u;
            } else if (request.output_name == TEXT("shield_cells_distance")) {
                expected_checksum = 2175695932u;
            }
            TestRunner->TestEqual(*FString::Printf(TEXT("%s checksum"), *request.output_name),
                                  pixel_checksum(generate_image(request)),
                                  expected_checksum);
        }
    }
};

TEST_CLASS(HexGridGenerator, "SandboxImages.UnitTests")
{
    TEST_METHOD(ProducesCleanLineAndBackgroundRegions)
    {
        auto const image{generate_hex_grid({.width = 96,
                                            .height = 64,
                                            .cell_radius = 14.0f,
                                            .line_thickness = 1.5f,
                                            .falloff = 1.0f})};

        int32 bright_pixel_count{0};
        int32 dark_pixel_count{0};
        int32 channel_mismatch_count{0};
        for (auto const& pixel : image.pixels) {
            bright_pixel_count += pixel.R >= 220 ? 1 : 0;
            dark_pixel_count += pixel.R <= 20 ? 1 : 0;
            channel_mismatch_count += pixel.R != pixel.A ? 1 : 0;
        }

        TestRunner->TestTrue(TEXT("Grid contains bright line pixels"), bright_pixel_count > 0);
        TestRunner->TestTrue(TEXT("Grid contains dark cell interiors"), dark_pixel_count > 0);
        TestRunner->TestEqual(TEXT("Grid RGB and alpha form one mask"), channel_mismatch_count, 0);
    }
};

}
