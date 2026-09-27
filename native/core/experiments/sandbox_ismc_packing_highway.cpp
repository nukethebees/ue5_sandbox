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
    return hn::ConvertTo(DInt{},
                         hn::Min(hn::Set(DFloat{}, 1023.0f), hn::Max(hn::Zero(DFloat{}), mapped)));
}
auto pack_rotation(FloatVec x, FloatVec y, FloatVec z, FloatVec w) noexcept -> IntVec {
    auto largest{hn::Zero(DInt{})};
    auto value{x};
    auto const select_y{hn::Gt(hn::Abs(y), hn::Abs(value))};
    largest = hn::IfThenElse(hn::RebindMask(DInt{}, select_y), hn::Set(DInt{}, 1), largest);
    value = hn::IfThenElse(select_y, y, value);
    auto const select_z{hn::Gt(hn::Abs(z), hn::Abs(value))};
    largest = hn::IfThenElse(hn::RebindMask(DInt{}, select_z), hn::Set(DInt{}, 2), largest);
    value = hn::IfThenElse(select_z, z, value);
    auto const select_w{hn::Gt(hn::Abs(w), hn::Abs(value))};
    largest = hn::IfThenElse(hn::RebindMask(DInt{}, select_w), hn::Set(DInt{}, 3), largest);
    value = hn::IfThenElse(select_w, w, value);

    auto const sign{hn::And(value, hn::Set(DFloat{}, -0.0f))};
    auto const a{hn::IfThenElse(hn::RebindMask(DFloat{}, hn::Eq(largest, hn::Zero(DInt{}))), y, x)};
    auto const b{
        hn::IfThenElse(hn::RebindMask(DFloat{}, hn::Lt(largest, hn::Set(DInt{}, 2))), z, y)};
    auto const c{
        hn::IfThenElse(hn::RebindMask(DFloat{}, hn::Lt(largest, hn::Set(DInt{}, 3))), w, z)};
    return hn::Or(largest,
                  hn::Or(hn::ShiftLeft<2>(quantize_component(hn::Xor(a, sign))),
                         hn::Or(hn::ShiftLeft<12>(quantize_component(hn::Xor(b, sign))),
                                hn::ShiftLeft<22>(quantize_component(hn::Xor(c, sign))))));
}

struct BatchBounds {
    FloatVec min_x;
    FloatVec min_y;
    FloatVec min_z;
    FloatVec max_x;
    FloatVec max_y;
    FloatVec max_z;
};

auto accumulate_axis(FloatVec position,
                     FloatVec a,
                     FloatVec b,
                     FloatVec c,
                     PackingParameters const& parameters,
                     FloatVec& minimum,
                     FloatVec& maximum) noexcept -> void {
    auto const center{
        hn::Add(hn::Add(hn::Add(position, hn::Mul(a, hn::Set(DFloat{}, parameters.mesh_origin.X))),
                        hn::Mul(b, hn::Set(DFloat{}, parameters.mesh_origin.Y))),
                hn::Mul(c, hn::Set(DFloat{}, parameters.mesh_origin.Z)))};
    auto const extent{
        hn::Add(hn::Add(hn::Mul(hn::Abs(a), hn::Set(DFloat{}, parameters.mesh_extent.X)),
                        hn::Mul(hn::Abs(b), hn::Set(DFloat{}, parameters.mesh_extent.Y))),
                hn::Mul(hn::Abs(c), hn::Set(DFloat{}, parameters.mesh_extent.Z)))};
    minimum = hn::Min(minimum, hn::Sub(center, extent));
    maximum = hn::Max(maximum, hn::Add(center, extent));
}
auto accumulate_bounds(FloatVec px,
                       FloatVec py,
                       FloatVec pz,
                       FloatVec x,
                       FloatVec y,
                       FloatVec z,
                       FloatVec w,
                       PackingParameters const& parameters,
                       BatchBounds& bounds) noexcept -> void {
    auto const two{hn::Set(DFloat{}, 2.0f)};
    auto const one{hn::Set(DFloat{}, 1.0f)};
    auto const x2{hn::Mul(two, x)};
    auto const y2{hn::Mul(two, y)};
    auto const z2{hn::Mul(two, z)};
    auto const w2{hn::Mul(two, w)};
    auto const xx{hn::Mul(x2, x)};
    auto const yy{hn::Mul(y2, y)};
    auto const zz{hn::Mul(z2, z)};
    auto const xy{hn::Mul(x2, y)};
    auto const xz{hn::Mul(x2, z)};
    auto const yz{hn::Mul(y2, z)};
    auto const wx{hn::Mul(w2, x)};
    auto const wy{hn::Mul(w2, y)};
    auto const wz{hn::Mul(w2, z)};
    accumulate_axis(px,
                    hn::Sub(one, hn::Add(yy, zz)),
                    hn::Sub(xy, wz),
                    hn::Add(xz, wy),
                    parameters,
                    bounds.min_x,
                    bounds.max_x);
    accumulate_axis(py,
                    hn::Add(xy, wz),
                    hn::Sub(one, hn::Add(xx, zz)),
                    hn::Sub(yz, wx),
                    parameters,
                    bounds.min_y,
                    bounds.max_y);
    accumulate_axis(pz,
                    hn::Sub(xz, wy),
                    hn::Add(yz, wx),
                    hn::Sub(one, hn::Add(xx, yy)),
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
        FloatVec px{};
        FloatVec py{};
        FloatVec pz{};
        std::array<std::int32_t, lanes> packed_x{};
        std::array<std::int32_t, lanes> packed_y{};
        std::array<std::int32_t, lanes> packed_z{};
        std::array<std::uint32_t, lanes> packed_rotation{};
        if constexpr (fields != PackingFields::Rotations) {
            px = load_axis<3, 0>(input.positions, index);
            py = load_axis<3, 1>(input.positions, index);
            pz = load_axis<3, 2>(input.positions, index);
            auto const x{quantize_position(px, parameters.position_root.X)};
            auto const y{quantize_position(py, parameters.position_root.Y)};
            auto const z{quantize_position(pz, parameters.position_root.Z)};
            hn::StoreU(x, DInt{}, packed_x.data());
            hn::StoreU(y, DInt{}, packed_y.data());
            hn::StoreU(z, DInt{}, packed_z.data());
        }
        if constexpr (fields != PackingFields::Positions) {
            auto const x{load_axis<4, 0>(input.rotations, index)};
            auto const y{load_axis<4, 1>(input.rotations, index)};
            auto const z{load_axis<4, 2>(input.rotations, index)};
            auto const w{load_axis<4, 3>(input.rotations, index)};
            auto const rotation{pack_rotation(x, y, z, w)};
            hn::StoreU(hn::BitCast(DUInt{}, rotation), DUInt{}, packed_rotation.data());
            if constexpr (bounds_mode == BoundsMode::Calculate) {
                accumulate_bounds(px, py, pz, x, y, z, w, parameters, batch);
            }
        }
        // The 12-byte AoS output has a reserved halfword that must stay untouched.
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
