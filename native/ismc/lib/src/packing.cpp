#include "packing_kernels.h"

#include <hwy/targets.h>

namespace ioj::ismc {
namespace packing_dispatch {
auto supports_highway_avx2() noexcept -> bool {
    // Highway checks OS vector state and its full AVX2 feature set, including FMA.
    static bool const supported{(hwy::SupportedTargets() & HWY_AVX2) != 0};
    return supported;
}
}

auto TransformPacker::pack_positions(std::span<std::byte const> positions,
                                     std::span<PackedTransform> output) const noexcept -> void {
    if (backend_ == PackingBackend::Production && packing_dispatch::supports_highway_avx2()) {
        highway_avx2::pack_positions(positions, parameters_.position_root, output);
    } else {
        scalar::pack_positions(positions, parameters_.position_root, output);
    }
}
auto TransformPacker::pack_rotations(std::span<std::byte const> rotations,
                                     std::span<PackedTransform> output) const noexcept -> void {
    if (backend_ == PackingBackend::Production && packing_dispatch::supports_highway_avx2()) {
        highway_avx2::pack_rotations(rotations, output);
    } else {
        scalar::pack_rotations(rotations, output);
    }
}
auto TransformPacker::pack_transforms(TransformInput input,
                                      std::span<PackedTransform> output) const noexcept
    -> TransformBounds {
    if (backend_ == PackingBackend::Production && packing_dispatch::supports_highway_avx2()) {
        return highway_avx2::pack_transforms(input, parameters_, output, bounds_mode_);
    } else {
        return scalar::pack_transforms(input, parameters_, output, bounds_mode_);
    }
}
}
