#pragma once

#include "ioj/ismc/packing.h"

namespace ioj::ismc::highway_avx2 {

auto pack_positions(std::span<std::byte const> positions,
                    Vector3f root,
                    std::span<PackedTransform> output) noexcept -> void;
auto pack_rotations(std::span<std::byte const> rotations,
                    std::span<PackedTransform> output) noexcept -> void;
auto pack_transforms(TransformInput input,
                     PackingParameters const& parameters,
                     std::span<PackedTransform> output,
                     BoundsMode mode) noexcept -> TransformBounds;
}

namespace ioj::ismc::scalar {
auto pack_positions(std::span<std::byte const> positions,
                    Vector3f root,
                    std::span<PackedTransform> output) noexcept -> void;
auto pack_rotations(std::span<std::byte const> rotations,
                    std::span<PackedTransform> output) noexcept -> void;
auto pack_transforms(TransformInput input,
                     PackingParameters const& parameters,
                     std::span<PackedTransform> output,
                     BoundsMode mode) noexcept -> TransformBounds;
}
