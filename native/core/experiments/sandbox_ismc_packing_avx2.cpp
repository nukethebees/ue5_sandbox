#include "sandbox_ismc_packing_avx2.h"

#include <immintrin.h>

#include <array>
#include <cassert>
#include <cstring>
#include <limits>

namespace ml::sandbox_ismc::experiment {
namespace avx2_detail {
template <std::size_t Components, std::size_t Axis>
auto load_axis(std::span<std::byte const> bytes, std::size_t index) noexcept -> __m256 {
    std::array<std::array<float, Components>, 8> values{};
    std::memcpy(values.data(), bytes.data() + index * Components * sizeof(float), sizeof(values));
    return _mm256_setr_ps(values[0][Axis],
                          values[1][Axis],
                          values[2][Axis],
                          values[3][Axis],
                          values[4][Axis],
                          values[5][Axis],
                          values[6][Axis],
                          values[7][Axis]);
}

auto absolute(__m256 value) noexcept -> __m256 {
    return _mm256_andnot_ps(_mm256_set1_ps(-0.0f), value);
}
auto quantize_position8(__m256 value, float root) noexcept -> __m256i {
    auto const offset{_mm256_mul_ps(_mm256_sub_ps(value, _mm256_set1_ps(root)),
                                    _mm256_set1_ps(inverse_position_quantum))};
    auto const integral{_mm256_cvttps_epi32(offset)};
    auto const fraction{_mm256_sub_ps(offset, _mm256_cvtepi32_ps(integral))};
    auto const increment{
        _mm256_castps_si256(_mm256_cmp_ps(fraction, _mm256_set1_ps(0.5f), _CMP_GE_OQ))};
    auto const decrement{
        _mm256_castps_si256(_mm256_cmp_ps(fraction, _mm256_set1_ps(-0.5f), _CMP_LT_OQ))};
    return _mm256_add_epi32(_mm256_sub_epi32(integral, increment), decrement);
}
auto quantize_component8(__m256 value) noexcept -> __m256i {
    auto const mapped{_mm256_add_ps(
        _mm256_mul_ps(_mm256_add_ps(value, _mm256_set1_ps(quaternion_component_limit)),
                      _mm256_set1_ps(1023.0f / (2.0f * quaternion_component_limit))),
        _mm256_set1_ps(0.5f))};
    return _mm256_cvttps_epi32(
        _mm256_min_ps(_mm256_set1_ps(1023.0f), _mm256_max_ps(_mm256_setzero_ps(), mapped)));
}
auto pack_rotation8(__m256 x, __m256 y, __m256 z, __m256 w) noexcept -> __m256i {
    auto largest{_mm256_setzero_si256()};
    auto value{x};
    auto const select_y{_mm256_cmp_ps(absolute(y), absolute(value), _CMP_GT_OQ)};
    largest = _mm256_blendv_epi8(largest, _mm256_set1_epi32(1), _mm256_castps_si256(select_y));
    value = _mm256_blendv_ps(value, y, select_y);
    auto const select_z{_mm256_cmp_ps(absolute(z), absolute(value), _CMP_GT_OQ)};
    largest = _mm256_blendv_epi8(largest, _mm256_set1_epi32(2), _mm256_castps_si256(select_z));
    value = _mm256_blendv_ps(value, z, select_z);
    auto const select_w{_mm256_cmp_ps(absolute(w), absolute(value), _CMP_GT_OQ)};
    largest = _mm256_blendv_epi8(largest, _mm256_set1_epi32(3), _mm256_castps_si256(select_w));
    value = _mm256_blendv_ps(value, w, select_w);

    auto const sign{_mm256_and_ps(value, _mm256_set1_ps(-0.0f))};
    auto const a{_mm256_blendv_ps(
        x, y, _mm256_castsi256_ps(_mm256_cmpeq_epi32(largest, _mm256_setzero_si256())))};
    auto const b{_mm256_blendv_ps(
        y, z, _mm256_castsi256_ps(_mm256_cmpgt_epi32(_mm256_set1_epi32(2), largest)))};
    auto const c{_mm256_blendv_ps(
        z, w, _mm256_castsi256_ps(_mm256_cmpgt_epi32(_mm256_set1_epi32(3), largest)))};
    return _mm256_or_si256(
        largest,
        _mm256_or_si256(
            _mm256_slli_epi32(quantize_component8(_mm256_xor_ps(a, sign)), 2),
            _mm256_or_si256(_mm256_slli_epi32(quantize_component8(_mm256_xor_ps(b, sign)), 12),
                            _mm256_slli_epi32(quantize_component8(_mm256_xor_ps(c, sign)), 22))));
}

struct Bounds8 {
    __m256 min_x;
    __m256 min_y;
    __m256 min_z;
    __m256 max_x;
    __m256 max_y;
    __m256 max_z;
};

auto accumulate_axis(__m256 position,
                     __m256 a,
                     __m256 b,
                     __m256 c,
                     PackingParameters const& parameters,
                     __m256& minimum,
                     __m256& maximum) noexcept -> void {
    auto const center{_mm256_add_ps(
        _mm256_add_ps(
            _mm256_add_ps(position, _mm256_mul_ps(a, _mm256_set1_ps(parameters.mesh_origin.X))),
            _mm256_mul_ps(b, _mm256_set1_ps(parameters.mesh_origin.Y))),
        _mm256_mul_ps(c, _mm256_set1_ps(parameters.mesh_origin.Z)))};
    auto const extent{_mm256_add_ps(
        _mm256_add_ps(_mm256_mul_ps(absolute(a), _mm256_set1_ps(parameters.mesh_extent.X)),
                      _mm256_mul_ps(absolute(b), _mm256_set1_ps(parameters.mesh_extent.Y))),
        _mm256_mul_ps(absolute(c), _mm256_set1_ps(parameters.mesh_extent.Z)))};
    minimum = _mm256_min_ps(minimum, _mm256_sub_ps(center, extent));
    maximum = _mm256_max_ps(maximum, _mm256_add_ps(center, extent));
}
auto accumulate_bounds(__m256 px,
                       __m256 py,
                       __m256 pz,
                       __m256 x,
                       __m256 y,
                       __m256 z,
                       __m256 w,
                       PackingParameters const& parameters,
                       Bounds8& bounds) noexcept -> void {
    auto const two{_mm256_set1_ps(2.0f)};
    auto const one{_mm256_set1_ps(1.0f)};
    auto const x2{_mm256_mul_ps(two, x)};
    auto const y2{_mm256_mul_ps(two, y)};
    auto const z2{_mm256_mul_ps(two, z)};
    auto const w2{_mm256_mul_ps(two, w)};
    auto const xx{_mm256_mul_ps(x2, x)};
    auto const yy{_mm256_mul_ps(y2, y)};
    auto const zz{_mm256_mul_ps(z2, z)};
    auto const xy{_mm256_mul_ps(x2, y)};
    auto const xz{_mm256_mul_ps(x2, z)};
    auto const yz{_mm256_mul_ps(y2, z)};
    auto const wx{_mm256_mul_ps(w2, x)};
    auto const wy{_mm256_mul_ps(w2, y)};
    auto const wz{_mm256_mul_ps(w2, z)};
    accumulate_axis(px,
                    _mm256_sub_ps(one, _mm256_add_ps(yy, zz)),
                    _mm256_sub_ps(xy, wz),
                    _mm256_add_ps(xz, wy),
                    parameters,
                    bounds.min_x,
                    bounds.max_x);
    accumulate_axis(py,
                    _mm256_add_ps(xy, wz),
                    _mm256_sub_ps(one, _mm256_add_ps(xx, zz)),
                    _mm256_sub_ps(yz, wx),
                    parameters,
                    bounds.min_y,
                    bounds.max_y);
    accumulate_axis(pz,
                    _mm256_sub_ps(xz, wy),
                    _mm256_add_ps(yz, wx),
                    _mm256_sub_ps(one, _mm256_add_ps(xx, yy)),
                    parameters,
                    bounds.min_z,
                    bounds.max_z);
}
auto reduce_axis(__m256 minimum, __m256 maximum, float& low, float& high) noexcept -> void {
    std::array<float, 8> minima{};
    std::array<float, 8> maxima{};
    _mm256_storeu_ps(minima.data(), minimum);
    _mm256_storeu_ps(maxima.data(), maximum);
    for (auto lane{0U}; lane < 8; ++lane) {
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
    auto const limit{_mm256_set1_ps(std::numeric_limits<float>::max())};
    auto const negative_limit{_mm256_set1_ps(-std::numeric_limits<float>::max())};
    Bounds8 batch{limit, limit, limit, negative_limit, negative_limit, negative_limit};
    auto const vector_count{output.size() / 8 * 8};
    for (std::size_t index{}; index < vector_count; index += 8) {
        __m256 px{};
        __m256 py{};
        __m256 pz{};
        std::array<std::int32_t, 8> packed_x{};
        std::array<std::int32_t, 8> packed_y{};
        std::array<std::int32_t, 8> packed_z{};
        std::array<std::uint32_t, 8> packed_rotation{};
        if constexpr (fields != PackingFields::Rotations) {
            px = load_axis<3, 0>(input.positions, index);
            py = load_axis<3, 1>(input.positions, index);
            pz = load_axis<3, 2>(input.positions, index);
            auto const x{quantize_position8(px, parameters.position_root.X)};
            auto const y{quantize_position8(py, parameters.position_root.Y)};
            auto const z{quantize_position8(pz, parameters.position_root.Z)};
            std::memcpy(packed_x.data(), &x, sizeof(x));
            std::memcpy(packed_y.data(), &y, sizeof(y));
            std::memcpy(packed_z.data(), &z, sizeof(z));
        }
        if constexpr (fields != PackingFields::Positions) {
            auto const x{load_axis<4, 0>(input.rotations, index)};
            auto const y{load_axis<4, 1>(input.rotations, index)};
            auto const z{load_axis<4, 2>(input.rotations, index)};
            auto const w{load_axis<4, 3>(input.rotations, index)};
            auto const rotation{pack_rotation8(x, y, z, w)};
            std::memcpy(packed_rotation.data(), &rotation, sizeof(rotation));
            if constexpr (bounds_mode == BoundsMode::Calculate) {
                accumulate_bounds(px, py, pz, x, y, z, w, parameters, batch);
            }
        }
        // The 12-byte AoS output has a reserved halfword that must stay untouched.
        for (auto lane{0U}; lane < 8; ++lane) {
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

auto pack_positions_avx2(std::span<std::byte const> positions,
                         Vector3f root,
                         std::span<PackedTransform> output) noexcept -> void {
    avx2_detail::pack<PackingFields::Positions, BoundsMode::Skip>(
        {positions, {}}, {root, {}, {}}, output, nullptr);
}
auto pack_rotations_avx2(std::span<std::byte const> rotations,
                         std::span<PackedTransform> output) noexcept -> void {
    avx2_detail::pack<PackingFields::Rotations, BoundsMode::Skip>(
        {{}, rotations}, {}, output, nullptr);
}
auto pack_transforms_avx2(TransformInput input,
                          PackingParameters const& parameters,
                          std::span<PackedTransform> output,
                          TransformBounds* bounds) noexcept -> void {
    if (bounds != nullptr) {
        avx2_detail::pack<PackingFields::Transforms, BoundsMode::Calculate>(
            input, parameters, output, bounds);
    } else {
        avx2_detail::pack<PackingFields::Transforms, BoundsMode::Skip>(
            input, parameters, output, nullptr);
    }
}
}
