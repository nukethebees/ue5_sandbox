#include "sandbox/core/sandbox_ismc_packing.h"

#include <array>
#include <cassert>
#include <cstring>
#include <limits>

namespace ml::sandbox_ismc {
namespace scalar_detail {
template <std::size_t Components>
auto load(std::span<std::byte const> bytes, std::size_t index) noexcept
    -> std::array<float, Components> {
    std::array<float, Components> result;
    std::memcpy(result.data(), bytes.data() + index * sizeof(result), sizeof(result));
    return result;
}

auto accumulate_bounds(std::array<float, 3> const& position,
                       std::array<float, 4> const& rotation,
                       PackingParameters const& parameters,
                       TransformBounds& bounds) noexcept -> void {
    auto const x{rotation[0]};
    auto const y{rotation[1]};
    auto const z{rotation[2]};
    auto const w{rotation[3]};
    auto const xx{2.0f * x * x};
    auto const yy{2.0f * y * y};
    auto const zz{2.0f * z * z};
    auto const xy{2.0f * x * y};
    auto const xz{2.0f * x * z};
    auto const yz{2.0f * y * z};
    auto const wx{2.0f * w * x};
    auto const wy{2.0f * w * y};
    auto const wz{2.0f * w * z};
    // Rows of R; valid also for the accepted small normalization error.
    std::array<std::array<float, 3>, 3> const rows{{{1.0f - (yy + zz), xy - wz, xz + wy},
                                                    {xy + wz, 1.0f - (xx + zz), yz - wx},
                                                    {xz - wy, yz + wx, 1.0f - (xx + yy)}}};
    for (auto axis{0U}; axis < 3; ++axis) {
        auto const& row{rows[axis]};
        auto const center{((position[axis] + row[0] * parameters.mesh_origin.X) +
                           row[1] * parameters.mesh_origin.Y) +
                          row[2] * parameters.mesh_origin.Z};
        auto const extent{(std::abs(row[0]) * parameters.mesh_extent.X +
                           std::abs(row[1]) * parameters.mesh_extent.Y) +
                          std::abs(row[2]) * parameters.mesh_extent.Z};
        bounds.minimum.Elements[axis] = std::min(bounds.minimum.Elements[axis], center - extent);
        bounds.maximum.Elements[axis] = std::max(bounds.maximum.Elements[axis], center + extent);
    }
}

template <bool CalculateBounds>
auto pack(TransformInput input,
          PackingParameters const& parameters,
          std::span<PackedTransform> output,
          TransformBounds* bounds) noexcept -> void {
    assert(input.positions.size() == output.size() * 3 * sizeof(float));
    assert(input.rotations.size() == output.size() * 4 * sizeof(float));
    TransformBounds batch{};
    if constexpr (CalculateBounds) {
        auto const limit{std::numeric_limits<float>::max()};
        batch = {make_vector3f(limit, limit, limit),
                 make_vector3f(-limit, -limit, -limit),
                 !output.empty()};
    }
    auto const count{output.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const position{load<3>(input.positions, index)};
        auto const rotation{load<4>(input.rotations, index)};
        auto& packed{output[index]};
        for (auto axis{0U}; axis < 3; ++axis) {
            packed.position[axis] = quantize_position_unchecked(
                position[axis], parameters.position_root.Elements[axis]);
        }
        packed.rotation =
            pack_normalized_quat32(rotation[0], rotation[1], rotation[2], rotation[3]);
        if constexpr (CalculateBounds) {
            accumulate_bounds(position, rotation, parameters, batch);
        }
    }
    if constexpr (CalculateBounds) {
        *bounds = batch;
    }
}
}

auto pack_positions_scalar(std::span<std::byte const> positions,
                           Vector3f root,
                           std::span<PackedTransform> output) noexcept -> void {
    assert(positions.size() == output.size() * 3 * sizeof(float));
    auto const count{output.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const position{scalar_detail::load<3>(positions, index)};
        for (auto axis{0U}; axis < 3; ++axis) {
            output[index].position[axis] =
                quantize_position_unchecked(position[axis], root.Elements[axis]);
        }
    }
}
auto pack_rotations_scalar(std::span<std::byte const> rotations,
                           std::span<PackedTransform> output) noexcept -> void {
    assert(rotations.size() == output.size() * 4 * sizeof(float));
    auto const count{output.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const rotation{scalar_detail::load<4>(rotations, index)};
        output[index].rotation =
            pack_normalized_quat32(rotation[0], rotation[1], rotation[2], rotation[3]);
    }
}
auto pack_transforms_scalar(TransformInput input,
                            PackingParameters const& parameters,
                            std::span<PackedTransform> output,
                            TransformBounds* bounds) noexcept -> void {
    if (bounds != nullptr) {
        scalar_detail::pack<true>(input, parameters, output, bounds);
    } else {
        scalar_detail::pack<false>(input, parameters, output, nullptr);
    }
}
}
