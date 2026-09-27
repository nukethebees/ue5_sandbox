#pragma once

#include "sandbox/core/sandbox_ismc_packing.h"

namespace ml::sandbox_ismc::experiment::highway_avx2 {

auto pack_positions(std::span<std::byte const> positions,
                    Vector3f root,
                    std::span<PackedTransform> output) noexcept -> void;
auto pack_rotations(std::span<std::byte const> rotations,
                    std::span<PackedTransform> output) noexcept -> void;
auto pack_transforms(TransformInput input,
                     PackingParameters const& parameters,
                     std::span<PackedTransform> output,
                     TransformBounds* bounds = nullptr) noexcept -> void;
}

namespace ml::sandbox_ismc::experiment::highway_avx512 {

auto pack_positions(std::span<std::byte const> positions,
                    Vector3f root,
                    std::span<PackedTransform> output) noexcept -> void;
auto pack_rotations(std::span<std::byte const> rotations,
                    std::span<PackedTransform> output) noexcept -> void;
auto pack_transforms(TransformInput input,
                     PackingParameters const& parameters,
                     std::span<PackedTransform> output,
                     TransformBounds* bounds = nullptr) noexcept -> void;
}
