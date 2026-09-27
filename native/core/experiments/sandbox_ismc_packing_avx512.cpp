#include "sandbox_ismc_packing_avx512.h"

#include <immintrin.h>

#include <array>
#include <cassert>
#include <cstring>
#include <limits>

namespace ml::sandbox_ismc::experiment {
namespace avx512_detail {
template <std::size_t Components, std::size_t Axis>
auto load_axis(std::span<std::byte const> bytes, std::size_t index) noexcept -> __m512 {
    std::array<std::array<float, Components>, 16> values{};
    std::memcpy(values.data(), bytes.data() + index * Components * sizeof(float), sizeof(values));
    return _mm512_setr_ps(values[0][Axis],
                          values[1][Axis],
                          values[2][Axis],
                          values[3][Axis],
                          values[4][Axis],
                          values[5][Axis],
                          values[6][Axis],
                          values[7][Axis],
                          values[8][Axis],
                          values[9][Axis],
                          values[10][Axis],
                          values[11][Axis],
                          values[12][Axis],
                          values[13][Axis],
                          values[14][Axis],
                          values[15][Axis]);
}

auto absolute(__m512 value) noexcept -> __m512 {
    return _mm512_castsi512_ps(_mm512_andnot_si512(
        _mm512_set1_epi32(std::numeric_limits<std::int32_t>::min()), _mm512_castps_si512(value)));
}
auto quantize_position16(__m512 value, float root) noexcept -> __m512i {
    auto const offset{_mm512_mul_ps(_mm512_sub_ps(value, _mm512_set1_ps(root)),
                                    _mm512_set1_ps(inverse_position_quantum))};
    auto const integral{_mm512_cvttps_epi32(offset)};
    auto const fraction{_mm512_sub_ps(offset, _mm512_cvtepi32_ps(integral))};
    auto const increment{_mm512_cmp_ps_mask(fraction, _mm512_set1_ps(0.5f), _CMP_GE_OQ)};
    auto const decrement{_mm512_cmp_ps_mask(fraction, _mm512_set1_ps(-0.5f), _CMP_LT_OQ)};
    auto const rounded_up{
        _mm512_mask_add_epi32(integral, increment, integral, _mm512_set1_epi32(1))};
    return _mm512_mask_sub_epi32(rounded_up, decrement, rounded_up, _mm512_set1_epi32(1));
}
auto quantize_component16(__m512 value) noexcept -> __m512i {
    auto const mapped{_mm512_add_ps(
        _mm512_mul_ps(_mm512_add_ps(value, _mm512_set1_ps(quaternion_component_limit)),
                      _mm512_set1_ps(1023.0f / (2.0f * quaternion_component_limit))),
        _mm512_set1_ps(0.5f))};
    return _mm512_cvttps_epi32(
        _mm512_min_ps(_mm512_set1_ps(1023.0f), _mm512_max_ps(_mm512_setzero_ps(), mapped)));
}
auto pack_rotation16(__m512 x, __m512 y, __m512 z, __m512 w) noexcept -> __m512i {
    auto largest{_mm512_setzero_si512()};
    auto value{x};
    auto const select_y{_mm512_cmp_ps_mask(absolute(y), absolute(value), _CMP_GT_OQ)};
    largest = _mm512_mask_mov_epi32(largest, select_y, _mm512_set1_epi32(1));
    value = _mm512_mask_blend_ps(select_y, value, y);
    auto const select_z{_mm512_cmp_ps_mask(absolute(z), absolute(value), _CMP_GT_OQ)};
    largest = _mm512_mask_mov_epi32(largest, select_z, _mm512_set1_epi32(2));
    value = _mm512_mask_blend_ps(select_z, value, z);
    auto const select_w{_mm512_cmp_ps_mask(absolute(w), absolute(value), _CMP_GT_OQ)};
    largest = _mm512_mask_mov_epi32(largest, select_w, _mm512_set1_epi32(3));
    value = _mm512_mask_blend_ps(select_w, value, w);

    auto const sign{_mm512_and_si512(_mm512_castps_si512(value),
                                     _mm512_set1_epi32(std::numeric_limits<std::int32_t>::min()))};
    auto const a{
        _mm512_mask_blend_ps(_mm512_cmpeq_epi32_mask(largest, _mm512_setzero_si512()), x, y)};
    auto const b{
        _mm512_mask_blend_ps(_mm512_cmplt_epi32_mask(largest, _mm512_set1_epi32(2)), y, z)};
    auto const c{
        _mm512_mask_blend_ps(_mm512_cmplt_epi32_mask(largest, _mm512_set1_epi32(3)), z, w)};
    auto const packed_a{
        quantize_component16(_mm512_castsi512_ps(_mm512_xor_si512(_mm512_castps_si512(a), sign)))};
    auto const packed_b{
        quantize_component16(_mm512_castsi512_ps(_mm512_xor_si512(_mm512_castps_si512(b), sign)))};
    auto const packed_c{
        quantize_component16(_mm512_castsi512_ps(_mm512_xor_si512(_mm512_castps_si512(c), sign)))};
    return _mm512_or_si512(largest,
                           _mm512_or_si512(_mm512_slli_epi32(packed_a, 2),
                                           _mm512_or_si512(_mm512_slli_epi32(packed_b, 12),
                                                           _mm512_slli_epi32(packed_c, 22))));
}

struct Bounds16 {
    __m512 min_x;
    __m512 min_y;
    __m512 min_z;
    __m512 max_x;
    __m512 max_y;
    __m512 max_z;
};

auto accumulate_axis(__m512 position,
                     __m512 a,
                     __m512 b,
                     __m512 c,
                     PackingParameters const& parameters,
                     __m512& minimum,
                     __m512& maximum) noexcept -> void {
    auto const center{_mm512_add_ps(
        _mm512_add_ps(
            _mm512_add_ps(position, _mm512_mul_ps(a, _mm512_set1_ps(parameters.mesh_origin.X))),
            _mm512_mul_ps(b, _mm512_set1_ps(parameters.mesh_origin.Y))),
        _mm512_mul_ps(c, _mm512_set1_ps(parameters.mesh_origin.Z)))};
    auto const extent{_mm512_add_ps(
        _mm512_add_ps(_mm512_mul_ps(absolute(a), _mm512_set1_ps(parameters.mesh_extent.X)),
                      _mm512_mul_ps(absolute(b), _mm512_set1_ps(parameters.mesh_extent.Y))),
        _mm512_mul_ps(absolute(c), _mm512_set1_ps(parameters.mesh_extent.Z)))};
    minimum = _mm512_min_ps(minimum, _mm512_sub_ps(center, extent));
    maximum = _mm512_max_ps(maximum, _mm512_add_ps(center, extent));
}
auto accumulate_bounds(__m512 px,
                       __m512 py,
                       __m512 pz,
                       __m512 x,
                       __m512 y,
                       __m512 z,
                       __m512 w,
                       PackingParameters const& parameters,
                       Bounds16& bounds) noexcept -> void {
    auto const two{_mm512_set1_ps(2.0f)};
    auto const one{_mm512_set1_ps(1.0f)};
    auto const x2{_mm512_mul_ps(two, x)};
    auto const y2{_mm512_mul_ps(two, y)};
    auto const z2{_mm512_mul_ps(two, z)};
    auto const w2{_mm512_mul_ps(two, w)};
    auto const xx{_mm512_mul_ps(x2, x)};
    auto const yy{_mm512_mul_ps(y2, y)};
    auto const zz{_mm512_mul_ps(z2, z)};
    auto const xy{_mm512_mul_ps(x2, y)};
    auto const xz{_mm512_mul_ps(x2, z)};
    auto const yz{_mm512_mul_ps(y2, z)};
    auto const wx{_mm512_mul_ps(w2, x)};
    auto const wy{_mm512_mul_ps(w2, y)};
    auto const wz{_mm512_mul_ps(w2, z)};
    accumulate_axis(px,
                    _mm512_sub_ps(one, _mm512_add_ps(yy, zz)),
                    _mm512_sub_ps(xy, wz),
                    _mm512_add_ps(xz, wy),
                    parameters,
                    bounds.min_x,
                    bounds.max_x);
    accumulate_axis(py,
                    _mm512_add_ps(xy, wz),
                    _mm512_sub_ps(one, _mm512_add_ps(xx, zz)),
                    _mm512_sub_ps(yz, wx),
                    parameters,
                    bounds.min_y,
                    bounds.max_y);
    accumulate_axis(pz,
                    _mm512_sub_ps(xz, wy),
                    _mm512_add_ps(yz, wx),
                    _mm512_sub_ps(one, _mm512_add_ps(xx, yy)),
                    parameters,
                    bounds.min_z,
                    bounds.max_z);
}
auto reduce_axis(__m512 minimum, __m512 maximum, float& low, float& high) noexcept -> void {
    std::array<float, 16> minima{};
    std::array<float, 16> maxima{};
    _mm512_storeu_ps(minima.data(), minimum);
    _mm512_storeu_ps(maxima.data(), maximum);
    for (auto lane{0U}; lane < 16; ++lane) {
        low = std::min(low, minima[lane]);
        high = std::max(high, maxima[lane]);
    }
}

template <bool Positions, bool Rotations, bool CalculateBounds>
auto pack(TransformInput input,
          PackingParameters const& parameters,
          std::span<PackedTransform> output,
          TransformBounds* bounds) noexcept -> void {
    assert(!Positions || input.positions.size() == output.size() * 3 * sizeof(float));
    assert(!Rotations || input.rotations.size() == output.size() * 4 * sizeof(float));
    auto const limit{_mm512_set1_ps(std::numeric_limits<float>::max())};
    auto const negative_limit{_mm512_set1_ps(-std::numeric_limits<float>::max())};
    Bounds16 batch{limit, limit, limit, negative_limit, negative_limit, negative_limit};
    auto const vector_count{output.size() / 16 * 16};
    for (std::size_t index{}; index < vector_count; index += 16) {
        __m512 px{};
        __m512 py{};
        __m512 pz{};
        std::array<std::int32_t, 16> packed_x{};
        std::array<std::int32_t, 16> packed_y{};
        std::array<std::int32_t, 16> packed_z{};
        std::array<std::uint32_t, 16> packed_rotation{};
        if constexpr (Positions) {
            px = load_axis<3, 0>(input.positions, index);
            py = load_axis<3, 1>(input.positions, index);
            pz = load_axis<3, 2>(input.positions, index);
            auto const x{quantize_position16(px, parameters.position_root.X)};
            auto const y{quantize_position16(py, parameters.position_root.Y)};
            auto const z{quantize_position16(pz, parameters.position_root.Z)};
            std::memcpy(packed_x.data(), &x, sizeof(x));
            std::memcpy(packed_y.data(), &y, sizeof(y));
            std::memcpy(packed_z.data(), &z, sizeof(z));
        }
        if constexpr (Rotations) {
            auto const x{load_axis<4, 0>(input.rotations, index)};
            auto const y{load_axis<4, 1>(input.rotations, index)};
            auto const z{load_axis<4, 2>(input.rotations, index)};
            auto const w{load_axis<4, 3>(input.rotations, index)};
            auto const rotation{pack_rotation16(x, y, z, w)};
            std::memcpy(packed_rotation.data(), &rotation, sizeof(rotation));
            if constexpr (CalculateBounds) {
                accumulate_bounds(px, py, pz, x, y, z, w, parameters, batch);
            }
        }
        // The 12-byte AoS output has a reserved halfword that must stay untouched.
        for (auto lane{0U}; lane < 16; ++lane) {
            auto& packed{output[index + lane]};
            if constexpr (Positions) {
                packed.position = {static_cast<std::int16_t>(packed_x[lane]),
                                   static_cast<std::int16_t>(packed_y[lane]),
                                   static_cast<std::int16_t>(packed_z[lane])};
            }
            if constexpr (Rotations) {
                packed.rotation.bits = packed_rotation[lane];
            }
        }
    }

    auto const tail{output.subspan(vector_count)};
    if constexpr (Positions && Rotations) {
        TransformInput const remaining{input.positions.subspan(vector_count * 3 * sizeof(float)),
                                       input.rotations.subspan(vector_count * 4 * sizeof(float))};
        pack_transforms_scalar(remaining, parameters, tail, bounds);
    } else if constexpr (Positions) {
        pack_positions_scalar(input.positions.subspan(vector_count * 3 * sizeof(float)),
                              parameters.position_root,
                              tail);
    } else {
        pack_rotations_scalar(input.rotations.subspan(vector_count * 4 * sizeof(float)), tail);
    }
    if constexpr (CalculateBounds) {
        reduce_axis(batch.min_x, batch.max_x, bounds->minimum.X, bounds->maximum.X);
        reduce_axis(batch.min_y, batch.max_y, bounds->minimum.Y, bounds->maximum.Y);
        reduce_axis(batch.min_z, batch.max_z, bounds->minimum.Z, bounds->maximum.Z);
        bounds->valid = !output.empty();
    }
}
}

auto pack_positions_avx512(std::span<std::byte const> positions,
                           Vector3f root,
                           std::span<PackedTransform> output) noexcept -> void {
    avx512_detail::pack<true, false, false>({positions, {}}, {root, {}, {}}, output, nullptr);
}
auto pack_rotations_avx512(std::span<std::byte const> rotations,
                           std::span<PackedTransform> output) noexcept -> void {
    avx512_detail::pack<false, true, false>({{}, rotations}, {}, output, nullptr);
}
auto pack_transforms_avx512(TransformInput input,
                            PackingParameters const& parameters,
                            std::span<PackedTransform> output,
                            TransformBounds* bounds) noexcept -> void {
    if (bounds != nullptr) {
        avx512_detail::pack<true, true, true>(input, parameters, output, bounds);
    } else {
        avx512_detail::pack<true, true, false>(input, parameters, output, nullptr);
    }
}
}
