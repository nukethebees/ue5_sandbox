#include "Generation/ImageGenerationUtilities.h"

namespace SandboxImages::GenLab {

auto FGeneratedImage::is_valid() const -> bool {
    return width > 0 && height > 0 && width <= MAX_int32 / height && pixels.Num() == width * height;
}

namespace ImageGeneration {

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

}

}
