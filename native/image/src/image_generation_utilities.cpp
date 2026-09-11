#include "image_generation_utilities.h"

#include <limits>

namespace sandbox::image {

auto GeneratedImage::is_valid() const -> bool {
    return width > 0 && height > 0 && width <= std::numeric_limits<std::int32_t>::max() / height &&
           pixels.size() == static_cast<std::size_t>(width * height);
}

namespace generation {

auto smooth_step(float const edge0, float const edge1, float const value) -> float {
    if (edge0 == edge1) {
        return value < edge0 ? 0.0f : 1.0f;
    }

    auto const alpha{std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f)};
    return alpha * alpha * (3.0f - 2.0f * alpha);
}

auto to_byte(float const value) -> std::uint8_t {
    return static_cast<std::uint8_t>(round_to_int(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

auto grayscale(float const value, std::uint8_t const alpha) -> Pixel {
    auto const intensity{to_byte(value)};
    return {intensity, intensity, intensity, alpha};
}

auto grayscale_mask(float const value) -> Pixel {
    auto const intensity{to_byte(value)};
    return {intensity, intensity, intensity, intensity};
}

auto make_image(std::int32_t const width, std::int32_t const height, Pixel const fill)
    -> GeneratedImage {
    GeneratedImage image{.width = width, .height = height, .pixels = {}, .error = {}};
    if (width <= 0 || height <= 0 || width > std::numeric_limits<std::int32_t>::max() / height) {
        image.error = "Image dimensions must be positive and fit in an int32 pixel count.";
        return image;
    }

    image.pixels.assign(static_cast<std::size_t>(width * height), fill);
    return image;
}

auto invalid_image(std::string const& error) -> GeneratedImage {
    return {.width = 0, .height = 0, .pixels = {}, .error = error};
}

auto normalized_distance(std::int32_t const x,
                         std::int32_t const y,
                         std::int32_t const width,
                         std::int32_t const height) -> float {
    auto const center_x{static_cast<float>(width - 1) * 0.5f};
    auto const center_y{static_cast<float>(height - 1) * 0.5f};
    auto const half_minimum_dimension{static_cast<float>(std::min(width, height)) * 0.5f};
    auto const dx{(static_cast<float>(x) - center_x) / half_minimum_dimension};
    auto const dy{(static_cast<float>(y) - center_y) / half_minimum_dimension};
    return std::sqrt(dx * dx + dy * dy);
}

}

}
