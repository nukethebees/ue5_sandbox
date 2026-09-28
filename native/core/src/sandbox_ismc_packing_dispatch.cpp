#include "sandbox_ismc_packing_highway.h"

#include <hwy/targets.h>

namespace ml::sandbox_ismc {
namespace packing_dispatch {
auto supports_highway_avx2() noexcept -> bool {
    // Highway checks OS vector state and its full AVX2 feature set, including FMA.
    static bool const supported{(hwy::SupportedTargets() & HWY_AVX2) != 0};
    return supported;
}
}

auto pack_positions(std::span<std::byte const> positions,
                    Vector3f root,
                    std::span<PackedTransform> output) noexcept -> void {
    if (packing_dispatch::supports_highway_avx2()) {
        highway_avx2::pack_positions(positions, root, output);
    } else {
        pack_positions_scalar(positions, root, output);
    }
}
auto pack_rotations(std::span<std::byte const> rotations,
                    std::span<PackedTransform> output) noexcept -> void {
    if (packing_dispatch::supports_highway_avx2()) {
        highway_avx2::pack_rotations(rotations, output);
    } else {
        pack_rotations_scalar(rotations, output);
    }
}
auto pack_transforms(TransformInput input,
                     PackingParameters const& params,
                     std::span<PackedTransform> output,
                     TransformBounds* bounds) noexcept -> void {
    if (packing_dispatch::supports_highway_avx2()) {
        highway_avx2::pack_transforms(input, params, output, bounds);
    } else {
        pack_transforms_scalar(input, params, output, bounds);
    }
}
}
