#pragma once

#include "Generation/ImageGenerators.h"

#include <CQTest.h>

namespace SandboxImages::GenLab::Tests {

inline auto pixel_at(FGeneratedImage const& image, int32 const x, int32 const y) -> FColor const& {
    return image.pixels[y * image.width + x];
}

inline void test_valid_image(FAutomationTestBase& test,
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

inline auto pixel_checksum(FGeneratedImage const& image) -> uint32 {
    uint32 checksum{2166136261u};
    for (auto const& pixel : image.pixels) {
        for (auto const channel : {pixel.R, pixel.G, pixel.B, pixel.A}) {
            checksum = (checksum ^ channel) * 16777619u;
        }
    }
    return checksum;
}

}
