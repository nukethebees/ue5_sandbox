#pragma once

#include "Generation/ImageGenerators.h"

namespace SandboxImages::GenLab::ImageGeneration {

auto smooth_step(float edge0, float edge1, float value) -> float;
auto to_byte(float value) -> uint8;
auto grayscale(float value, uint8 alpha) -> FColor;
auto grayscale_mask(float value) -> FColor;
auto make_image(int32 width, int32 height, FColor fill) -> FGeneratedImage;
auto invalid_image(FString const& error) -> FGeneratedImage;
auto normalized_distance(int32 x, int32 y, int32 width, int32 height) -> float;

}
