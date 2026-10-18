#pragma once

#include "ioj/ismc/transform.h"

#include <limits>
#include <span>

namespace ioj::ismc {

// Production input is two AoS arrays: tightly packed float XYZ and float XYZW.
// Byte views allow Unreal and native callers to share their object representations
// without aliasing unrelated vector/quaternion types or copying whole batches.
// Inputs must be finite, positions quantizable, rotations normalized, and input
// and output storage disjoint. Input sizes must match the output instance count.
struct TransformInput {
    std::span<std::byte const> positions;
    std::span<std::byte const> rotations;
};

struct PackingParameters {
    Vector3f position_root{};
    Vector3f mesh_origin{};
    Vector3f mesh_extent{};
};

struct TransformBounds {
    inline static constexpr float limit{std::numeric_limits<float>::max()};
    Vector3f minimum{limit, limit, limit};
    Vector3f maximum{-limit, -limit, -limit};

    // An inverted interval represents no bounds, including an empty batch.
    [[nodiscard]] auto empty() const noexcept -> bool {
        return minimum.x() > maximum.x() || minimum.y() > maximum.y() || minimum.z() > maximum.z();
    }
};

enum class PackingFields { Positions, Rotations, Transforms };
enum class BoundsMode { Skip, Calculate };
enum class PackingBackend { Production, Scalar };

// Partial operations write only their respective fields. All operations preserve reserved.
// Production selects Highway AVX2 when the CPU and OS support it, otherwise scalar.
class TransformPacker {
  public:
    explicit TransformPacker(PackingParameters parameters,
                             BoundsMode bounds_mode = BoundsMode::Skip,
                             PackingBackend backend = PackingBackend::Production) noexcept
        : parameters_{parameters}
        , bounds_mode_{bounds_mode}
        , backend_{backend} {}

    auto pack_positions(std::span<std::byte const> positions,
                        std::span<PackedTransform> output) const noexcept -> void;
    auto pack_rotations(std::span<std::byte const> rotations,
                        std::span<PackedTransform> output) const noexcept -> void;
    // Return empty bounds when skipped or when output is empty.
    auto pack_transforms(TransformInput input, std::span<PackedTransform> output) const noexcept
        -> TransformBounds;
  private:
    PackingParameters parameters_;
    BoundsMode bounds_mode_;
    PackingBackend backend_;
};
}
