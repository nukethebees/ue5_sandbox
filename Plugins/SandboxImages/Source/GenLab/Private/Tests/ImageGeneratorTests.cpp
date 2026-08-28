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
                         TEXT("hex grid"),
                         generate_hex_grid({.width = width, .height = height}),
                         width,
                         height);
    }

    TEST_METHOD(RejectsInvalidParametersWithUsefulErrors)
    {
        auto const invalid_dimensions{generate_radial_gradient({.width = 0})};
        auto const invalid_ring{generate_ring_mask({.thickness = 0.0f})};
        auto const invalid_starfield{generate_starfield({.minimum_radius = -1.0f})};
        auto const invalid_noise{generate_noise({.base_scale = 0.0f})};
        auto const invalid_hex{generate_hex_grid({.cell_radius = 0.0f})};

        for (auto const* image : {&invalid_dimensions,
                                  &invalid_ring,
                                  &invalid_starfield,
                                  &invalid_noise,
                                  &invalid_hex}) {
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
        TestRunner->TestEqual(
            TEXT("There is one request per proof-of-concept generator"), requests.Num(), 5);

        TSet<FString> output_names;
        for (auto const& request : requests) {
            TestRunner->TestFalse(TEXT("Default output name is present"),
                                  request.output_name.IsEmpty());
            output_names.Add(request.output_name);
            TestRunner->TestTrue(TEXT("Default request generates a valid image"),
                                 generate_image(request).is_valid());
            TestRunner->TestTrue(TEXT("Request description contains its format version"),
                                 describe_request(request).StartsWith(TEXT("version=1;")));
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
