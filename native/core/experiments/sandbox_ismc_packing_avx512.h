#pragma once

#include "sandbox/core/sandbox_ismc_packing.h"

#include <cpuinfo_x86.h>

namespace ml::sandbox_ismc::experiment {
// Test/benchmark-only entry points, never linked into the production native library.
// Check the full compiler ISA target before calling any of these kernels.
inline auto supports_avx512() -> bool {
    auto const features{cpu_features::GetX86Info().features};
    return features.avx512f != 0 && features.avx512dq != 0 && features.avx512cd != 0 &&
           features.avx512bw != 0 && features.avx512vl != 0;
}

auto pack_positions_avx512(std::span<std::byte const> positions,
                           Vector3f root,
                           std::span<PackedTransform> output) noexcept -> void;
auto pack_rotations_avx512(std::span<std::byte const> rotations,
                           std::span<PackedTransform> output) noexcept -> void;
auto pack_transforms_avx512(TransformInput input,
                            PackingParameters const& parameters,
                            std::span<PackedTransform> output,
                            TransformBounds* bounds = nullptr) noexcept -> void;
}
