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

template <std::size_t Components, std::size_t Axis>
auto load_axis(std::span<std::byte const> bytes, std::size_t index) noexcept -> FloatVec {
    // Copy byte-backed inputs without assuming alignment or typed aliases.
    std::array<std::array<float, Components>, lanes> values{};
    std::memcpy(values.data(), bytes.data() + index * Components * sizeof(float), sizeof(values));

    std::array<float, lanes> axis{};
    for (std::size_t lane{}; lane < lanes; ++lane) {
        axis[lane] = values[lane][Axis];
    }

    return hn::LoadU(DFloat{}, axis.data());
}

auto quantize_position(FloatVec value, float root) noexcept -> IntVec {
    auto const offset{hn::Mul(hn::Sub(value, hn::Set(DFloat{}, root)),
                              hn::Set(DFloat{}, inverse_position_quantum))};

    // Round to the nearest integer; break ties toward positive infinity.
    auto const integral{hn::ConvertTo(DInt{}, offset)};
    auto const fraction{hn::Sub(offset, hn::ConvertTo(DFloat{}, integral))};

    auto const increment{
        hn::IfThenElse(hn::RebindMask(DInt{}, hn::Ge(fraction, hn::Set(DFloat{}, 0.5f))),
                       hn::Set(DInt{}, 1),
                       hn::Zero(DInt{}))};
    auto const decrement{
        hn::IfThenElse(hn::RebindMask(DInt{}, hn::Lt(fraction, hn::Set(DFloat{}, -0.5f))),
                       hn::Set(DInt{}, 1),
                       hn::Zero(DInt{}))};

    return hn::Sub(hn::Add(integral, increment), decrement);
}
auto quantize_component(FloatVec value) noexcept -> IntVec {
    auto const mapped{
        hn::Add(hn::Mul(hn::Add(value, hn::Set(DFloat{}, quaternion_component_limit)),
                        hn::Set(DFloat{}, 1023.0f / (2.0f * quaternion_component_limit))),
                hn::Set(DFloat{}, 0.5f))};

    // Clamp normalization drift before converting to ten bits.
    return hn::ConvertTo(DInt{},
                         hn::Min(hn::Set(DFloat{}, 1023.0f), hn::Max(hn::Zero(DFloat{}), mapped)));
}
auto pack_rotation(FloatVec quaternion_x,
                   FloatVec quaternion_y,
                   FloatVec quaternion_z,
                   FloatVec quaternion_w) noexcept -> IntVec {
    auto largest_component_index{hn::Zero(DInt{})};
    auto largest_component{quaternion_x};

    // Keep the first component when magnitudes tie.
    auto const select_y{hn::Gt(hn::Abs(quaternion_y), hn::Abs(largest_component))};
    largest_component_index = hn::IfThenElse(
        hn::RebindMask(DInt{}, select_y), hn::Set(DInt{}, 1), largest_component_index);
    largest_component = hn::IfThenElse(select_y, quaternion_y, largest_component);

    auto const select_z{hn::Gt(hn::Abs(quaternion_z), hn::Abs(largest_component))};
    largest_component_index = hn::IfThenElse(
        hn::RebindMask(DInt{}, select_z), hn::Set(DInt{}, 2), largest_component_index);
    largest_component = hn::IfThenElse(select_z, quaternion_z, largest_component);

    auto const select_w{hn::Gt(hn::Abs(quaternion_w), hn::Abs(largest_component))};
    largest_component_index = hn::IfThenElse(
        hn::RebindMask(DInt{}, select_w), hn::Set(DInt{}, 3), largest_component_index);
    largest_component = hn::IfThenElse(select_w, quaternion_w, largest_component);

    // Omit the largest component and canonicalize its sign.
    auto const sign_mask{hn::And(largest_component, hn::Set(DFloat{}, -0.0f))};
    auto const first_retained_component{
        hn::IfThenElse(hn::RebindMask(DFloat{}, hn::Eq(largest_component_index, hn::Zero(DInt{}))),
                       quaternion_y,
                       quaternion_x)};
    auto const second_retained_component{hn::IfThenElse(
        hn::RebindMask(DFloat{}, hn::Lt(largest_component_index, hn::Set(DInt{}, 2))),
        quaternion_z,
        quaternion_y)};
    auto const third_retained_component{hn::IfThenElse(
        hn::RebindMask(DFloat{}, hn::Lt(largest_component_index, hn::Set(DInt{}, 3))),
        quaternion_w,
        quaternion_z)};

    // Store the index in two bits and each retained component in ten.
    return hn::Or(
        largest_component_index,
        hn::Or(hn::ShiftLeft<2>(quantize_component(hn::Xor(first_retained_component, sign_mask))),
               hn::Or(hn::ShiftLeft<12>(
                          quantize_component(hn::Xor(second_retained_component, sign_mask))),
                      hn::ShiftLeft<22>(
                          quantize_component(hn::Xor(third_retained_component, sign_mask))))));
}

struct BatchBounds {
    FloatVec min_x;
    FloatVec min_y;
    FloatVec min_z;

    FloatVec max_x;
    FloatVec max_y;
    FloatVec max_z;
};

auto accumulate_axis(FloatVec position_axis,
                     FloatVec rotation_row_x,
                     FloatVec rotation_row_y,
                     FloatVec rotation_row_z,
                     PackingParameters const& parameters,
                     FloatVec& minimum,
                     FloatVec& maximum) noexcept -> void {
    auto const center{hn::Add(
        hn::Add(hn::Add(position_axis,
                        hn::Mul(rotation_row_x, hn::Set(DFloat{}, parameters.mesh_origin.X))),
                hn::Mul(rotation_row_y, hn::Set(DFloat{}, parameters.mesh_origin.Y))),
        hn::Mul(rotation_row_z, hn::Set(DFloat{}, parameters.mesh_origin.Z)))};

    // Project the local AABB extents using the absolute rotation basis.
    auto const extent{hn::Add(
        hn::Add(hn::Mul(hn::Abs(rotation_row_x), hn::Set(DFloat{}, parameters.mesh_extent.X)),
                hn::Mul(hn::Abs(rotation_row_y), hn::Set(DFloat{}, parameters.mesh_extent.Y))),
        hn::Mul(hn::Abs(rotation_row_z), hn::Set(DFloat{}, parameters.mesh_extent.Z)))};

    minimum = hn::Min(minimum, hn::Sub(center, extent));
    maximum = hn::Max(maximum, hn::Add(center, extent));
}
auto accumulate_bounds(FloatVec position_x,
                       FloatVec position_y,
                       FloatVec position_z,
                       FloatVec quaternion_x,
                       FloatVec quaternion_y,
                       FloatVec quaternion_z,
                       FloatVec quaternion_w,
                       PackingParameters const& parameters,
                       BatchBounds& bounds) noexcept -> void {
    auto const two{hn::Set(DFloat{}, 2.0f)};
    auto const one{hn::Set(DFloat{}, 1.0f)};

    // Preserve scalar multiplication order for exact bounds parity.
    auto const twice_x{hn::Mul(two, quaternion_x)};
    auto const twice_y{hn::Mul(two, quaternion_y)};
    auto const twice_z{hn::Mul(two, quaternion_z)};
    auto const twice_w{hn::Mul(two, quaternion_w)};

    auto const twice_xx{hn::Mul(twice_x, quaternion_x)};
    auto const twice_yy{hn::Mul(twice_y, quaternion_y)};
    auto const twice_zz{hn::Mul(twice_z, quaternion_z)};

    auto const twice_xy{hn::Mul(twice_x, quaternion_y)};
    auto const twice_xz{hn::Mul(twice_x, quaternion_z)};
    auto const twice_yz{hn::Mul(twice_y, quaternion_z)};

    auto const twice_wx{hn::Mul(twice_w, quaternion_x)};
    auto const twice_wy{hn::Mul(twice_w, quaternion_y)};
    auto const twice_wz{hn::Mul(twice_w, quaternion_z)};

    // Expand the quaternion into rotation rows.
    accumulate_axis(position_x,
                    hn::Sub(one, hn::Add(twice_yy, twice_zz)),
                    hn::Sub(twice_xy, twice_wz),
                    hn::Add(twice_xz, twice_wy),
                    parameters,
                    bounds.min_x,
                    bounds.max_x);

    accumulate_axis(position_y,
                    hn::Add(twice_xy, twice_wz),
                    hn::Sub(one, hn::Add(twice_xx, twice_zz)),
                    hn::Sub(twice_yz, twice_wx),
                    parameters,
                    bounds.min_y,
                    bounds.max_y);

    accumulate_axis(position_z,
                    hn::Sub(twice_xz, twice_wy),
                    hn::Add(twice_yz, twice_wx),
                    hn::Sub(one, hn::Add(twice_xx, twice_yy)),
                    parameters,
                    bounds.min_z,
                    bounds.max_z);
}
auto reduce_axis(FloatVec minimum, FloatVec maximum, float& low, float& high) noexcept -> void {
    std::array<float, lanes> minima{};
    std::array<float, lanes> maxima{};
    hn::StoreU(minimum, DFloat{}, minima.data());
    hn::StoreU(maximum, DFloat{}, maxima.data());

    for (auto lane{0U}; lane < lanes; ++lane) {
        low = std::min(low, minima[lane]);
        high = std::max(high, maxima[lane]);
    }
}

template <PackingFields fields, BoundsMode bounds_mode>
auto pack(TransformInput input,
          PackingParameters const& parameters,
          std::span<PackedTransform> output,
          TransformBounds* bounds) noexcept -> void {
    static_assert(bounds_mode == BoundsMode::Skip || fields == PackingFields::Transforms);
    assert(fields == PackingFields::Rotations ||
           input.positions.size() == output.size() * 3 * sizeof(float));
    assert(fields == PackingFields::Positions ||
           input.rotations.size() == output.size() * 4 * sizeof(float));

    auto const limit{hn::Set(DFloat{}, std::numeric_limits<float>::max())};
    auto const negative_limit{hn::Set(DFloat{}, -std::numeric_limits<float>::max())};
    BatchBounds batch{limit, limit, limit, negative_limit, negative_limit, negative_limit};

    auto const vector_count{output.size() / lanes * lanes};
    for (std::size_t index{}; index < vector_count; index += lanes) {
        FloatVec position_x{};
        FloatVec position_y{};
        FloatVec position_z{};

        std::array<std::int32_t, lanes> packed_x{};
        std::array<std::int32_t, lanes> packed_y{};
        std::array<std::int32_t, lanes> packed_z{};
        std::array<std::uint32_t, lanes> packed_rotation{};

        if constexpr (fields != PackingFields::Rotations) {
            position_x = load_axis<3, 0>(input.positions, index);
            position_y = load_axis<3, 1>(input.positions, index);
            position_z = load_axis<3, 2>(input.positions, index);

            auto const quantized_x{quantize_position(position_x, parameters.position_root.X)};
            auto const quantized_y{quantize_position(position_y, parameters.position_root.Y)};
            auto const quantized_z{quantize_position(position_z, parameters.position_root.Z)};

            hn::StoreU(quantized_x, DInt{}, packed_x.data());
            hn::StoreU(quantized_y, DInt{}, packed_y.data());
            hn::StoreU(quantized_z, DInt{}, packed_z.data());
        }

        if constexpr (fields != PackingFields::Positions) {
            auto const quaternion_x{load_axis<4, 0>(input.rotations, index)};
            auto const quaternion_y{load_axis<4, 1>(input.rotations, index)};
            auto const quaternion_z{load_axis<4, 2>(input.rotations, index)};
            auto const quaternion_w{load_axis<4, 3>(input.rotations, index)};

            auto const rotation{
                pack_rotation(quaternion_x, quaternion_y, quaternion_z, quaternion_w)};
            hn::StoreU(hn::BitCast(DUInt{}, rotation), DUInt{}, packed_rotation.data());

            if constexpr (bounds_mode == BoundsMode::Calculate) {
                accumulate_bounds(position_x,
                                  position_y,
                                  position_z,
                                  quaternion_x,
                                  quaternion_y,
                                  quaternion_z,
                                  quaternion_w,
                                  parameters,
                                  batch);
            }
        }

        // Preserve the reserved halfword in each 12-byte output.
        for (auto lane{0U}; lane < lanes; ++lane) {
            auto& packed{output[index + lane]};
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
    auto const tail{output.subspan(vector_count)};
    if constexpr (fields == PackingFields::Transforms) {
        TransformInput const remaining{input.positions.subspan(vector_count * 3 * sizeof(float)),
                                       input.rotations.subspan(vector_count * 4 * sizeof(float))};
        pack_transforms_scalar(remaining, parameters, tail, bounds);
    } else if constexpr (fields == PackingFields::Positions) {
        pack_positions_scalar(input.positions.subspan(vector_count * 3 * sizeof(float)),
                              parameters.position_root,
                              tail);
    } else {
        pack_rotations_scalar(input.rotations.subspan(vector_count * 4 * sizeof(float)), tail);
    }

    if constexpr (bounds_mode == BoundsMode::Calculate) {
        // Merge vector extrema into the scalar tail bounds.
        reduce_axis(batch.min_x, batch.max_x, bounds->minimum.X, bounds->maximum.X);
        reduce_axis(batch.min_y, batch.max_y, bounds->minimum.Y, bounds->maximum.Y);
        reduce_axis(batch.min_z, batch.max_z, bounds->minimum.Z, bounds->maximum.Z);

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
                     PackingParameters const& parameters,
                     std::span<PackedTransform> output,
                     TransformBounds* bounds) noexcept -> void {
    if (bounds != nullptr) {
        detail::pack<PackingFields::Transforms, BoundsMode::Calculate>(
            input, parameters, output, bounds);
    } else {
        detail::pack<PackingFields::Transforms, BoundsMode::Skip>(
            input, parameters, output, nullptr);
    }
}
}

HWY_AFTER_NAMESPACE();
#undef IOJ_HIGHWAY_NAMESPACE
