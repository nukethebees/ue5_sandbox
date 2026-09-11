#pragma once

#include "sandbox/image/image_generation.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <type_traits>

#define TEXT(value) value

struct TestLabel {
    [[nodiscard]] auto operator*() const -> char const* { return ""; }
};

struct FString {
    template <typename... Args>
    [[nodiscard]] static auto Printf(char const*, Args&&...) -> TestLabel {
        return {};
    }
};

struct TestRunnerAdapter {
    void TestTrue(char const*, bool const value) const { EXPECT_TRUE(value); }

    void TestFalse(char const*, bool const value) const { EXPECT_FALSE(value); }

    template <typename Actual, typename Expected>
    void TestEqual(char const*, Actual const& actual, Expected const& expected) const {
        if constexpr (std::is_integral_v<Actual> && std::is_integral_v<Expected>) {
            using Common = std::common_type_t<Actual, Expected>;
            EXPECT_EQ(static_cast<Common>(actual), static_cast<Common>(expected));
        } else {
            EXPECT_EQ(actual, expected);
        }
    }
};

inline TestRunnerAdapter test_runner;
inline TestRunnerAdapter* TestRunner{&test_runner};

[[nodiscard]] inline auto round_to_int(float const value) -> std::int32_t {
    return static_cast<std::int32_t>(std::floor(value + 0.5f));
}

[[nodiscard]] inline auto
    is_nearly_equal(float const left, float const right, float const tolerance) -> bool {
    return std::abs(left - right) <= tolerance;
}

namespace sandbox::image::tests {

inline auto pixel_at(GeneratedImage const& image, std::int32_t const x, std::int32_t const y)
    -> Pixel const& {
    return image.pixels[static_cast<std::size_t>(y * image.width + x)];
}

inline void test_valid_image(TestRunnerAdapter& test,
                             char const* const name,
                             GeneratedImage const& image,
                             std::int32_t const expected_width,
                             std::int32_t const expected_height) {
    test.TestTrue(name, image.is_valid());
    test.TestEqual(name, image.width, expected_width);
    test.TestEqual(name, image.height, expected_height);
    test.TestEqual(name, image.pixels.size(), expected_width * expected_height);
}

inline auto pixel_checksum(GeneratedImage const& image) -> std::uint32_t {
    std::uint32_t checksum{2166136261u};
    for (auto const& pixel : image.pixels) {
        for (auto const channel : {pixel.red, pixel.green, pixel.blue, pixel.alpha}) {
            checksum = (checksum ^ channel) * 16777619u;
        }
    }
    return checksum;
}

}
