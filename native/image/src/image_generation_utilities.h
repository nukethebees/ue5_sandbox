#pragma once

#include "sandbox/image/image_generation.h"

#include <algorithm>
#include <cmath>

namespace sandbox::image::generation {

auto smooth_step(float edge0, float edge1, float value) -> float;
auto to_byte(float value) -> std::uint8_t;
auto grayscale(float value, std::uint8_t alpha) -> Pixel;
auto grayscale_mask(float value) -> Pixel;
auto make_image(std::int32_t width, std::int32_t height, Pixel fill) -> GeneratedImage;
auto invalid_image(std::string const& error) -> GeneratedImage;
auto normalized_distance(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height)
    -> float;

[[nodiscard]] inline auto floor_to_int(float const value) -> std::int32_t {
    return static_cast<std::int32_t>(std::floor(value));
}

[[nodiscard]] inline auto ceil_to_int(float const value) -> std::int32_t {
    return static_cast<std::int32_t>(std::ceil(value));
}

[[nodiscard]] inline auto round_to_int(float const value) -> std::int32_t {
    return floor_to_int(value + 0.5f);
}

template <typename T>
[[nodiscard]] constexpr auto lerp(T const start, T const end, float const alpha) -> T {
    return start + (end - start) * alpha;
}

}
