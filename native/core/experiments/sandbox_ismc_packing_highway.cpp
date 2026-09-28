#include "sandbox_ismc_packing_highway.h"

#include <hwy/highway.h>

#include <array>
#include <cassert>
#include <cstring>
#include <limits>

#if HWY_TARGET == HWY_AVX2
#define IOJ_HIGHWAY_NAMESPACE highway_avx2
#elif HWY_TARGET == HWY_AVX3
#define IOJ_HIGHWAY_NAMESPACE highway_avx512
#else
#error Unsupported Highway packing comparison target
#endif

HWY_BEFORE_NAMESPACE();

namespace ml::sandbox_ismc::experiment::IOJ_HIGHWAY_NAMESPACE {
namespace detail {

namespace hn = hwy::HWY_NAMESPACE;

using DFloat = hn::ScalableTag<float>;
using DInt = hn::RebindToSigned<DFloat>;
using DUInt = hn::RebindToUnsigned<DFloat>;

using FloatVec = hn::Vec<DFloat>;
using IntVec = hn::Vec<DInt>;

inline constexpr auto lanes{HWY_LANES(float)};
static_assert(lanes == (HWY_TARGET == HWY_AVX2 ? 8 : 16));

struct Vec3Batch {
    FloatVec x, y, z;
};

struct QuatBatch {
    FloatVec x, y, z, w;
};

auto quantize_position(FloatVec value, FloatVec root) noexcept -> IntVec {
    auto const offset{(value - root) * hn::Set(DFloat{}, inverse_position_quantum)};

    // Quantizable positions fit int16, so the int32 conversion needs no saturation.
    // Round to the nearest integer; break ties toward positive infinity.
    auto const integral{hn::ConvertInRangeTo(DInt{}, offset)};
    auto const fraction{offset - hn::ConvertTo(DFloat{}, integral)};

    auto const increment{
        hn::IfThenElse(hn::RebindMask(DInt{}, hn::Ge(fraction, hn::Set(DFloat{}, 0.5f))),
                       hn::Set(DInt{}, 1),
                       hn::Zero(DInt{}))};
    auto const decrement{
        hn::IfThenElse(hn::RebindMask(DInt{}, hn::Lt(fraction, hn::Set(DFloat{}, -0.5f))),
                       hn::Set(DInt{}, 1),
                       hn::Zero(DInt{}))};

    return integral + increment - decrement;
}
auto quantize_component(FloatVec value) noexcept -> IntVec {
    auto const mapped{(value + hn::Set(DFloat{}, quaternion_component_limit)) *
                          hn::Set(DFloat{}, 1023.0f / (2.0f * quaternion_component_limit)) +
                      hn::Set(DFloat{}, 0.5f)};

    // Clamp normalization drift before converting to ten bits.
    return hn::ConvertTo(DInt{},
                         hn::Min(hn::Set(DFloat{}, 1023.0f), hn::Max(hn::Zero(DFloat{}), mapped)));
}
auto pack_rotation(QuatBatch q) noexcept -> IntVec {
    auto largest_index{hn::Zero(DInt{})};
    auto largest{q.x};

    // Keep the first component when magnitudes tie.
    auto const select_y{hn::Gt(hn::Abs(q.y), hn::Abs(largest))};
    largest_index =
        hn::IfThenElse(hn::RebindMask(DInt{}, select_y), hn::Set(DInt{}, 1), largest_index);
    largest = hn::IfThenElse(select_y, q.y, largest);

    auto const select_z{hn::Gt(hn::Abs(q.z), hn::Abs(largest))};
    largest_index =
        hn::IfThenElse(hn::RebindMask(DInt{}, select_z), hn::Set(DInt{}, 2), largest_index);
    largest = hn::IfThenElse(select_z, q.z, largest);

    auto const select_w{hn::Gt(hn::Abs(q.w), hn::Abs(largest))};
    largest_index =
        hn::IfThenElse(hn::RebindMask(DInt{}, select_w), hn::Set(DInt{}, 3), largest_index);
    largest = hn::IfThenElse(select_w, q.w, largest);

    // Omit the largest component and canonicalize its sign.
    auto const sign_mask{hn::And(largest, hn::Set(DFloat{}, -0.0f))};
    auto const retained0{hn::IfThenElse(
        hn::RebindMask(DFloat{}, hn::Eq(largest_index, hn::Zero(DInt{}))), q.y, q.x)};
    auto const retained1{hn::IfThenElse(
        hn::RebindMask(DFloat{}, hn::Lt(largest_index, hn::Set(DInt{}, 2))), q.z, q.y)};
    auto const retained2{hn::IfThenElse(
        hn::RebindMask(DFloat{}, hn::Lt(largest_index, hn::Set(DInt{}, 3))), q.w, q.z)};

    auto const a{quantize_component(hn::Xor(retained0, sign_mask))};
    auto const b{quantize_component(hn::Xor(retained1, sign_mask))};
    auto const c{quantize_component(hn::Xor(retained2, sign_mask))};

    // Store the index in two bits and each retained component in ten.
    return largest_index | hn::ShiftLeft<2>(a) | hn::ShiftLeft<12>(b) | hn::ShiftLeft<22>(c);
}

struct BatchBounds {
    Vec3Batch min;
    Vec3Batch max;
};

auto accumulate_bounds(Vec3Batch p,
                       QuatBatch q,
                       Vec3Batch origin,
                       Vec3Batch extent,
                       BatchBounds& bounds) noexcept -> void {
    auto const two{hn::Set(DFloat{}, 2.0f)};
    auto const one{hn::Set(DFloat{}, 1.0f)};

    // Preserve scalar evaluation order for exact bounds parity; do not fuse Mul/Add.
    auto const x2{two * q.x};
    auto const y2{two * q.y};
    auto const z2{two * q.z};
    auto const w2{two * q.w};

    auto const xx{x2 * q.x}, yy{y2 * q.y}, zz{z2 * q.z};
    auto const xy{x2 * q.y}, xz{x2 * q.z}, yz{y2 * q.z};
    auto const wx{w2 * q.x}, wy{w2 * q.y}, wz{w2 * q.z};

    // Rows of R, including the accepted small normalization error.
    Vec3Batch const r0{one - (yy + zz), xy - wz, xz + wy};
    Vec3Batch const r1{xy + wz, one - (xx + zz), yz - wx};
    Vec3Batch const r2{xz - wy, yz + wx, one - (xx + yy)};

    Vec3Batch const c{((p.x + r0.x * origin.x) + r0.y * origin.y) + r0.z * origin.z,
                      ((p.y + r1.x * origin.x) + r1.y * origin.y) + r1.z * origin.z,
                      ((p.z + r2.x * origin.x) + r2.y * origin.y) + r2.z * origin.z};

    // Project local AABB extents using |R|.
    Vec3Batch const e{
        (hn::Abs(r0.x) * extent.x + hn::Abs(r0.y) * extent.y) + hn::Abs(r0.z) * extent.z,
        (hn::Abs(r1.x) * extent.x + hn::Abs(r1.y) * extent.y) + hn::Abs(r1.z) * extent.z,
        (hn::Abs(r2.x) * extent.x + hn::Abs(r2.y) * extent.y) + hn::Abs(r2.z) * extent.z};

    bounds.min.x = hn::Min(bounds.min.x, c.x - e.x);
    bounds.min.y = hn::Min(bounds.min.y, c.y - e.y);
    bounds.min.z = hn::Min(bounds.min.z, c.z - e.z);

    bounds.max.x = hn::Max(bounds.max.x, c.x + e.x);
    bounds.max.y = hn::Max(bounds.max.y, c.y + e.y);
    bounds.max.z = hn::Max(bounds.max.z, c.z + e.z);
}
auto reduce_axis(FloatVec minimum, FloatVec maximum, float& low, float& high) noexcept -> void {
    low = std::min(low, hn::ReduceMin(DFloat{}, minimum));
    high = std::max(high, hn::ReduceMax(DFloat{}, maximum));
}

template <PackingFields fields, BoundsMode bounds_mode>
auto pack(TransformInput input,
          PackingParameters const& params,
          std::span<PackedTransform> output,
          TransformBounds* bounds) noexcept -> void {
    static_assert(bounds_mode == BoundsMode::Skip || fields == PackingFields::Transforms);
    assert(fields == PackingFields::Rotations ||
           input.positions.size() == output.size() * 3 * sizeof(float));
    assert(fields == PackingFields::Positions ||
           input.rotations.size() == output.size() * 4 * sizeof(float));

    auto const limit{hn::Set(DFloat{}, std::numeric_limits<float>::max())};
    auto const negative_limit{hn::Set(DFloat{}, -std::numeric_limits<float>::max())};
    BatchBounds batch{{limit, limit, limit}, {negative_limit, negative_limit, negative_limit}};
    Vec3Batch const origin{hn::Set(DFloat{}, params.mesh_origin.X),
                           hn::Set(DFloat{}, params.mesh_origin.Y),
                           hn::Set(DFloat{}, params.mesh_origin.Z)};
    Vec3Batch const extent{hn::Set(DFloat{}, params.mesh_extent.X),
                           hn::Set(DFloat{}, params.mesh_extent.Y),
                           hn::Set(DFloat{}, params.mesh_extent.Z)};
    Vec3Batch const root{hn::Set(DFloat{}, params.position_root.X),
                         hn::Set(DFloat{}, params.position_root.Y),
                         hn::Set(DFloat{}, params.position_root.Z)};

    auto* const dst{output.data()};
    auto const simd_end{output.size() / lanes * lanes};
    for (std::size_t index{}; index < simd_end; index += lanes) {
        Vec3Batch p{};

        std::array<std::int32_t, lanes> packed_x{};
        std::array<std::int32_t, lanes> packed_y{};
        std::array<std::int32_t, lanes> packed_z{};
        std::array<std::uint32_t, lanes> packed_rotation{};

        if constexpr (fields != PackingFields::Rotations) {
            // Copy object representations without assuming alignment or typed aliases.
            std::array<float, 3 * lanes> xyz{};
            std::memcpy(
                xyz.data(), input.positions.data() + index * 3 * sizeof(float), sizeof(xyz));
            hn::LoadInterleaved3(DFloat{}, xyz.data(), p.x, p.y, p.z);

            auto const quantized_x{quantize_position(p.x, root.x)};
            auto const quantized_y{quantize_position(p.y, root.y)};
            auto const quantized_z{quantize_position(p.z, root.z)};

            hn::StoreU(quantized_x, DInt{}, packed_x.data());
            hn::StoreU(quantized_y, DInt{}, packed_y.data());
            hn::StoreU(quantized_z, DInt{}, packed_z.data());
        }

        if constexpr (fields != PackingFields::Positions) {
            std::array<float, 4 * lanes> xyzw{};
            std::memcpy(
                xyzw.data(), input.rotations.data() + index * 4 * sizeof(float), sizeof(xyzw));
            QuatBatch q{};
            hn::LoadInterleaved4(DFloat{}, xyzw.data(), q.x, q.y, q.z, q.w);

            auto const rotation{pack_rotation(q)};
            hn::StoreU(hn::BitCast(DUInt{}, rotation), DUInt{}, packed_rotation.data());

            if constexpr (bounds_mode == BoundsMode::Calculate) {
                accumulate_bounds(p, q, origin, extent, batch);
            }
        }

        // Preserve the reserved halfword in each 12-byte output.
        for (auto lane{0U}; lane < lanes; ++lane) {
            auto& packed{dst[index + lane]};
            if constexpr (fields != PackingFields::Rotations) {
                packed.position = {static_cast<std::int16_t>(packed_x[lane]),
                                   static_cast<std::int16_t>(packed_y[lane]),
                                   static_cast<std::int16_t>(packed_z[lane])};
            }

            if constexpr (fields != PackingFields::Positions) {
                packed.rotation.bits = packed_rotation[lane];
            }
        }
    }

    // Pack the remainder without reading beyond the input spans.
    auto const tail{output.subspan(simd_end)};
    if constexpr (fields == PackingFields::Transforms) {
        TransformInput const remaining{input.positions.subspan(simd_end * 3 * sizeof(float)),
                                       input.rotations.subspan(simd_end * 4 * sizeof(float))};
        pack_transforms_scalar(remaining, params, tail, bounds);
    } else if constexpr (fields == PackingFields::Positions) {
        pack_positions_scalar(
            input.positions.subspan(simd_end * 3 * sizeof(float)), params.position_root, tail);
    } else {
        pack_rotations_scalar(input.rotations.subspan(simd_end * 4 * sizeof(float)), tail);
    }

    if constexpr (bounds_mode == BoundsMode::Calculate) {
        // Merge vector extrema into the scalar tail bounds.
        reduce_axis(batch.min.x, batch.max.x, bounds->minimum.X, bounds->maximum.X);
        reduce_axis(batch.min.y, batch.max.y, bounds->minimum.Y, bounds->maximum.Y);
        reduce_axis(batch.min.z, batch.max.z, bounds->minimum.Z, bounds->maximum.Z);

        bounds->valid = !output.empty();
    }
}
}

auto pack_positions(std::span<std::byte const> positions,
                    Vector3f root,
                    std::span<PackedTransform> output) noexcept -> void {
    detail::pack<PackingFields::Positions, BoundsMode::Skip>(
        {positions, {}}, {root, {}, {}}, output, nullptr);
}
auto pack_rotations(std::span<std::byte const> rotations,
                    std::span<PackedTransform> output) noexcept -> void {
    detail::pack<PackingFields::Rotations, BoundsMode::Skip>({{}, rotations}, {}, output, nullptr);
}
auto pack_transforms(TransformInput input,
                     PackingParameters const& params,
                     std::span<PackedTransform> output,
                     TransformBounds* bounds) noexcept -> void {
    if (bounds != nullptr) {
        detail::pack<PackingFields::Transforms, BoundsMode::Calculate>(
            input, params, output, bounds);
    } else {
        detail::pack<PackingFields::Transforms, BoundsMode::Skip>(input, params, output, nullptr);
    }
}
}

HWY_AFTER_NAMESPACE();
#undef IOJ_HIGHWAY_NAMESPACE
