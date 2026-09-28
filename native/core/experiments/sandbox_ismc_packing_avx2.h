#pragma once

#include "sandbox/core/sandbox_ismc_packing.h"

namespace ml::sandbox_ismc::experiment {

auto pack_positions_avx2(std::span<std::byte const> positions,
                         Vector3f root,
                         std::span<PackedTransform> output) noexcept -> void;
auto pack_rotations_avx2(std::span<std::byte const> rotations,
                         std::span<PackedTransform> output) noexcept -> void;
auto pack_transforms_avx2(TransformInput input,
                          PackingParameters const& parameters,
                          std::span<PackedTransform> output,
                          TransformBounds* bounds = nullptr) noexcept -> void;
}
