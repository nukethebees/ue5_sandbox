#pragma once

#include "sandbox/core/sandbox_ismc_transform.h"

#include <span>

namespace ml::sandbox_ismc {

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
    Vector3f minimum{};
    Vector3f maximum{};
    bool valid{};
};

enum class PackingFields { Positions, Rotations, Transforms };
enum class BoundsMode { Skip, Calculate };

// Partial kernels write only their respective fields. All kernels preserve reserved.
auto pack_positions_scalar(std::span<std::byte const> positions,
                           Vector3f root,
                           std::span<PackedTransform> output) noexcept -> void;
auto pack_rotations_scalar(std::span<std::byte const> rotations,
                           std::span<PackedTransform> output) noexcept -> void;
auto pack_transforms_scalar(TransformInput input,
                            PackingParameters const& parameters,
                            std::span<PackedTransform> output,
                            TransformBounds* bounds = nullptr) noexcept -> void;

// Explicit AVX2 entry points: caller must establish AVX2 support (the Unreal
// module already requires AVX2). Scalar entry points remain independently usable.
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
