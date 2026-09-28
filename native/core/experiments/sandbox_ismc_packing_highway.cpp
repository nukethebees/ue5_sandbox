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

using DF = hn::ScalableTag<float>;
using DI = hn::RebindToSigned<DF>;
using DU = hn::RebindToUnsigned<DF>;

inline constexpr DF df{};
inline constexpr DI di{};
inline constexpr DU du{};

using VF = hn::Vec<DF>;
using VI = hn::Vec<DI>;

inline constexpr auto lanes{HWY_LANES(float)};
static_assert(lanes == (HWY_TARGET == HWY_AVX2 ? 8 : 16));

inline constexpr auto index_bits{2};
inline constexpr auto component_bits{10};
inline constexpr float component_max{static_cast<float>((1U << component_bits) - 1U)};

struct Vec3Batch {
    VF x;
    VF y;
    VF z;
};

struct QuatBatch {
    VF x;
    VF y;
    VF z;
    VF w;
};

auto quantize_position(VF value, VF root) noexcept -> VI {
    auto const offset{(value - root) * hn::Set(df, inverse_position_quantum)};

    // Quantizable positions fit int16, so the int32 conversion needs no saturation.
    // Round to the nearest integer; break ties toward positive infinity.
    auto const integral{hn::ConvertInRangeTo(di, offset)};
    auto const fraction{offset - hn::ConvertTo(df, integral)};

    auto const increment{hn::IfThenElse(
        hn::RebindMask(di, hn::Ge(fraction, hn::Set(df, 0.5f))), hn::Set(di, 1), hn::Zero(di))};
    auto const decrement{hn::IfThenElse(
        hn::RebindMask(di, hn::Lt(fraction, hn::Set(df, -0.5f))), hn::Set(di, 1), hn::Zero(di))};

    return integral + increment - decrement;
}
auto quantize_component(VF value) noexcept -> VI {
    constexpr auto scale{component_max / (2.0f * quaternion_component_limit)};
    auto const mapped{(value + hn::Set(df, quaternion_component_limit)) * hn::Set(df, scale) +
                      hn::Set(df, 0.5f)};

    // Clamp normalization drift before converting to ten bits.
    return hn::ConvertTo(di, hn::Min(hn::Set(df, component_max), hn::Max(hn::Zero(df), mapped)));
}
auto pack_rotation(QuatBatch q) noexcept -> VI {
    auto largest_index{hn::Zero(di)};
    auto largest{q.x};

    // Keep the first component when magnitudes tie.
    auto const select_y{hn::Gt(hn::Abs(q.y), hn::Abs(largest))};
    largest_index = hn::IfThenElse(hn::RebindMask(di, select_y), hn::Set(di, 1), largest_index);
    largest = hn::IfThenElse(select_y, q.y, largest);

    auto const select_z{hn::Gt(hn::Abs(q.z), hn::Abs(largest))};
    largest_index = hn::IfThenElse(hn::RebindMask(di, select_z), hn::Set(di, 2), largest_index);
    largest = hn::IfThenElse(select_z, q.z, largest);

    auto const select_w{hn::Gt(hn::Abs(q.w), hn::Abs(largest))};
    largest_index = hn::IfThenElse(hn::RebindMask(di, select_w), hn::Set(di, 3), largest_index);
    largest = hn::IfThenElse(select_w, q.w, largest);

    // Omit the largest component and canonicalize its sign.
    auto const sign_mask{hn::And(largest, hn::Set(df, -0.0f))};
    auto const retained0{
        hn::IfThenElse(hn::RebindMask(df, hn::Eq(largest_index, hn::Zero(di))), q.y, q.x)};
    auto const retained1{
        hn::IfThenElse(hn::RebindMask(df, hn::Lt(largest_index, hn::Set(di, 2))), q.z, q.y)};
    auto const retained2{
        hn::IfThenElse(hn::RebindMask(df, hn::Lt(largest_index, hn::Set(di, 3))), q.w, q.z)};

    auto const a{quantize_component(hn::Xor(retained0, sign_mask))};
    auto const b{quantize_component(hn::Xor(retained1, sign_mask))};
    auto const c{quantize_component(hn::Xor(retained2, sign_mask))};

    // Store the index in two bits and each retained component in ten.
    return largest_index | hn::ShiftLeft<index_bits>(a) |
           hn::ShiftLeft<index_bits + component_bits>(b) |
           hn::ShiftLeft<index_bits + 2 * component_bits>(c);
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
    auto const two{hn::Set(df, 2.0f)};
    auto const one{hn::Set(df, 1.0f)};

    // Preserve scalar multiplication order in the quaternion products.
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

    Vec3Batch const c{
        hn::MulAdd(r0.z, origin.z, hn::MulAdd(r0.y, origin.y, hn::MulAdd(r0.x, origin.x, p.x))),
        hn::MulAdd(r1.z, origin.z, hn::MulAdd(r1.y, origin.y, hn::MulAdd(r1.x, origin.x, p.y))),
        hn::MulAdd(r2.z, origin.z, hn::MulAdd(r2.y, origin.y, hn::MulAdd(r2.x, origin.x, p.z)))};

    // Project local AABB extents using |R|.
    Vec3Batch const e{
        hn::MulAdd(
            hn::Abs(r0.z), extent.z, hn::MulAdd(hn::Abs(r0.y), extent.y, hn::Abs(r0.x) * extent.x)),
        hn::MulAdd(
            hn::Abs(r1.z), extent.z, hn::MulAdd(hn::Abs(r1.y), extent.y, hn::Abs(r1.x) * extent.x)),
        hn::MulAdd(hn::Abs(r2.z),
                   extent.z,
                   hn::MulAdd(hn::Abs(r2.y), extent.y, hn::Abs(r2.x) * extent.x))};

    bounds.min.x = hn::Min(bounds.min.x, c.x - e.x);
    bounds.min.y = hn::Min(bounds.min.y, c.y - e.y);
    bounds.min.z = hn::Min(bounds.min.z, c.z - e.z);

    bounds.max.x = hn::Max(bounds.max.x, c.x + e.x);
    bounds.max.y = hn::Max(bounds.max.y, c.y + e.y);
    bounds.max.z = hn::Max(bounds.max.z, c.z + e.z);
}
auto reduce_axis(VF minimum, VF maximum, float& low, float& high) noexcept -> void {
    low = std::min(low, hn::ReduceMin(df, minimum));
    high = std::max(high, hn::ReduceMax(df, maximum));
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

    auto const limit{hn::Set(df, std::numeric_limits<float>::max())};
    auto const negative_limit{hn::Set(df, -std::numeric_limits<float>::max())};
    BatchBounds batch{{limit, limit, limit}, {negative_limit, negative_limit, negative_limit}};
    Vec3Batch const origin{hn::Set(df, params.mesh_origin.X),
                           hn::Set(df, params.mesh_origin.Y),
                           hn::Set(df, params.mesh_origin.Z)};
    Vec3Batch const extent{hn::Set(df, params.mesh_extent.X),
                           hn::Set(df, params.mesh_extent.Y),
                           hn::Set(df, params.mesh_extent.Z)};
    Vec3Batch const root{hn::Set(df, params.position_root.X),
                         hn::Set(df, params.position_root.Y),
                         hn::Set(df, params.position_root.Z)};

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
            hn::LoadInterleaved3(df, xyz.data(), p.x, p.y, p.z);

            auto const quantized_x{quantize_position(p.x, root.x)};
            auto const quantized_y{quantize_position(p.y, root.y)};
            auto const quantized_z{quantize_position(p.z, root.z)};

            hn::StoreU(quantized_x, di, packed_x.data());
            hn::StoreU(quantized_y, di, packed_y.data());
            hn::StoreU(quantized_z, di, packed_z.data());
        }

        if constexpr (fields != PackingFields::Positions) {
            std::array<float, 4 * lanes> xyzw{};
            std::memcpy(
                xyzw.data(), input.rotations.data() + index * 4 * sizeof(float), sizeof(xyzw));
            QuatBatch q{};
            hn::LoadInterleaved4(df, xyzw.data(), q.x, q.y, q.z, q.w);

            auto const rotation{pack_rotation(q)};
            hn::StoreU(hn::BitCast(du, rotation), du, packed_rotation.data());

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
