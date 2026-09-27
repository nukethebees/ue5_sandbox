#include "native/generated/add_scaled_x86_simd_lab.h"
#include "native/generated/dot_product_3d_x86_simd_lab.h"
#include "native/generated/dot_product_x86_simd_lab.h"

#include <cpuinfo_x86.h>
#include <gtest/gtest.h>
#include <hwy/targets.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

namespace add = ml::kernel_benchmark;
namespace dot = ml::kernel_benchmark::dot_product_lab;

using AddKernel = void (*)(float const*, float const*, float, float*, std::int32_t) noexcept;
using AddChunkKernel = void (*)(add::FloatChunk16 const*,
                                add::FloatChunk16 const*,
                                float,
                                add::FloatChunk16*,
                                std::int32_t) noexcept;
using DotKernel = float (*)(float const*, float const*, std::int32_t) noexcept;
using DotChunkKernel = float (*)(dot::FloatChunk16 const*,
                                 dot::FloatChunk16 const*,
                                 std::int32_t) noexcept;

auto has_avx512() -> bool {
    auto const features{cpu_features::GetX86Info().features};
    return features.avx512f && features.avx512cd && features.avx512bw && features.avx512dq &&
           features.avx512vl;
}

void fill_values(float* const lhs, float* const rhs, std::int32_t const count) {
    for (std::int32_t index{}; index < count; ++index) {
        lhs[index] = static_cast<float>((index * 13) % 257) * 0.031f - 4.0f;
        rhs[index] = static_cast<float>((index * 29) % 251) * 0.017f - 2.0f;
    }
}

void expect_add(AddKernel const kernel, std::int32_t const count) {
    for (auto const offset : {0, 1}) {
        alignas(64) std::array<float, 272> base{};
        alignas(64) std::array<float, 272> value{};
        alignas(64) std::array<float, 272> out{};
        out.fill(12345.0f);
        fill_values(base.data() + offset, value.data() + offset, count);
        kernel(base.data() + offset, value.data() + offset, -0.75f, out.data() + offset, count);
        for (std::int32_t index{}; index < count; ++index) {
            EXPECT_EQ(out[index + offset], base[index + offset] + value[index + offset] * -0.75f);
        }
        if (offset != 0) {
            EXPECT_EQ(out[0], 12345.0f);
        }
        EXPECT_EQ(out[count + offset], 12345.0f);
    }
}

void expect_chunk_add(AddChunkKernel const kernel, std::int32_t const count) {
    using Chunk = add::FloatChunk16;
    auto const chunk_count{(count + Chunk::capacity - 1) / Chunk::capacity};
    std::vector<Chunk> base(static_cast<std::size_t>(chunk_count));
    std::vector<Chunk> value(static_cast<std::size_t>(chunk_count));
    std::vector<Chunk> out(static_cast<std::size_t>(chunk_count));
    for (std::int32_t index{}; index < chunk_count * Chunk::capacity; ++index) {
        auto const chunk{index / Chunk::capacity};
        auto const lane{index % Chunk::capacity};
        base[chunk].values[lane] = static_cast<float>(index) * 0.25f;
        value[chunk].values[lane] = static_cast<float>(index % 7) - 3.0f;
    }
    kernel(base.data(), value.data(), 1.5f, out.data(), chunk_count);
    for (std::int32_t index{}; index < chunk_count * Chunk::capacity; ++index) {
        auto const chunk{index / Chunk::capacity};
        auto const lane{index % Chunk::capacity};
        EXPECT_EQ(out[chunk].values[lane],
                  base[chunk].values[lane] + value[chunk].values[lane] * 1.5f);
    }
}

auto reference_dot(float const* const lhs, float const* const rhs, std::int32_t const count)
    -> double {
    auto result{0.0};
    for (std::int32_t index{}; index < count; ++index) {
        result += static_cast<double>(lhs[index]) * static_cast<double>(rhs[index]);
    }
    return result;
}

void expect_dot(DotKernel const kernel, std::int32_t const count) {
    for (auto const offset : {0, 1}) {
        alignas(64) std::array<float, 272> lhs{};
        alignas(64) std::array<float, 272> rhs{};
        fill_values(lhs.data() + offset, rhs.data() + offset, count);
        auto const expected{reference_dot(lhs.data() + offset, rhs.data() + offset, count)};
        auto const actual{kernel(lhs.data() + offset, rhs.data() + offset, count)};
        auto const tolerance{std::max(1.0e-5, std::abs(expected) * 2.0e-5)};
        EXPECT_NEAR(static_cast<double>(actual), expected, tolerance);
    }
}

void expect_chunk_dot(DotChunkKernel const kernel, std::int32_t const count) {
    using Chunk = dot::FloatChunk16;
    auto const chunk_count{(count + Chunk::capacity - 1) / Chunk::capacity};
    std::vector<Chunk> lhs(static_cast<std::size_t>(chunk_count));
    std::vector<Chunk> rhs(static_cast<std::size_t>(chunk_count));
    for (std::int32_t index{}; index < count; ++index) {
        auto const chunk{index / Chunk::capacity};
        auto const lane{index % Chunk::capacity};
        lhs[chunk].values[lane] = static_cast<float>((index * 13) % 257) * 0.031f - 4.0f;
        rhs[chunk].values[lane] = static_cast<float>((index * 29) % 251) * 0.017f - 2.0f;
    }
    auto expected{0.0};
    for (std::int32_t index{}; index < count; ++index) {
        auto const chunk{index / Chunk::capacity};
        auto const lane{index % Chunk::capacity};
        expected += static_cast<double>(lhs[chunk].values[lane]) *
                    static_cast<double>(rhs[chunk].values[lane]);
    }
    auto const actual{kernel(lhs.data(), rhs.data(), chunk_count)};
    auto const tolerance{std::max(1.0e-5, std::abs(expected) * 2.0e-5)};
    EXPECT_NEAR(static_cast<double>(actual), expected, tolerance);
}

TEST(NativeSimdLab, FloatChunksHaveOneAlignedCacheLinePerOperand) {
    EXPECT_EQ(sizeof(add::FloatChunk16), 64);
    EXPECT_EQ(alignof(add::FloatChunk16), 64);
    std::vector<add::FloatChunk16> chunks(3);
    for (auto const& chunk : chunks) {
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(chunk.values.data()) % 64u, 0u);
    }
}

TEST(NativeSimdLab, AddBackendsMatchTheExpressionAcrossVectorTails) {
    std::array<AddKernel, 4> const avx2_kernels{add::backend::scalar::add_scaled,
                                                add::backend::autovec_avx2::add_scaled,
                                                add::backend::avx2::add_scaled,
                                                add::backend::avx2_unrolled::add_scaled};
    for (auto const kernel : avx2_kernels) {
        for (auto const count : {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 257}) {
            expect_add(kernel, count);
        }
    }
    if (has_avx512()) {
        std::array<AddKernel, 2> const avx512_kernels{add::backend::autovec_avx512::add_scaled,
                                                      add::backend::avx512::add_scaled};
        for (auto const kernel : avx512_kernels) {
            for (auto const count : {0, 1, 15, 16, 17, 31, 32, 33, 257}) {
                expect_add(kernel, count);
            }
        }
    }
}

TEST(NativeSimdLab, ChunkAddProcessesEveryPhysicalLane) {
    std::array const avx2_kernels{
        static_cast<AddChunkKernel>(add::backend::scalar::add_scaled),
        static_cast<AddChunkKernel>(add::backend::autovec_avx2::add_scaled),
        static_cast<AddChunkKernel>(add::backend::avx2::add_scaled),
        static_cast<AddChunkKernel>(add::backend::avx2_unrolled::add_scaled)};
    for (auto const kernel : avx2_kernels) {
        for (auto const count : {1, 8, 11, 16, 17, 32, 257}) {
            expect_chunk_add(kernel, count);
        }
    }
    if (has_avx512()) {
        expect_chunk_add(static_cast<AddChunkKernel>(add::backend::autovec_avx512::add_scaled),
                         257);
        expect_chunk_add(static_cast<AddChunkKernel>(add::backend::avx512::add_scaled), 257);
    }
}

TEST(NativeSimdLab, StrictAndRelaxedDotBackendsRemainNumericallySound) {
    std::array<DotKernel, 2> const strict_avx2{dot::strict::backend::scalar::dot_product,
                                               dot::strict::backend::autovec_avx2::dot_product};
    std::array<DotKernel, 3> const relaxed_avx2{dot::relaxed::backend::autovec_avx2::dot_product,
                                                dot::relaxed::backend::avx2::dot_product,
                                                dot::relaxed::backend::avx2_unrolled::dot_product};
    for (auto const kernel : strict_avx2) {
        for (auto const count : {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 257}) {
            expect_dot(kernel, count);
        }
    }
    for (auto const kernel : relaxed_avx2) {
        for (auto const count : {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 257}) {
            expect_dot(kernel, count);
        }
    }
    if (has_avx512()) {
        expect_dot(dot::strict::backend::autovec_avx512::dot_product, 257);
        expect_dot(dot::relaxed::backend::autovec_avx512::dot_product, 257);
        expect_dot(dot::relaxed::backend::avx512::dot_product, 257);
        expect_dot(dot::relaxed::backend::avx512_unrolled::dot_product, 257);
    }
}

TEST(NativeSimdLab, ChunkDotUsesZeroPaddingForPartialChunks) {
    std::array const avx2_kernels{
        static_cast<DotChunkKernel>(dot::strict::backend::scalar::dot_product),
        static_cast<DotChunkKernel>(dot::strict::backend::autovec_avx2::dot_product),
        static_cast<DotChunkKernel>(dot::relaxed::backend::autovec_avx2::dot_product),
        static_cast<DotChunkKernel>(dot::relaxed::backend::avx2::dot_product),
        static_cast<DotChunkKernel>(dot::relaxed::backend::avx2_unrolled::dot_product)};
    for (auto const kernel : avx2_kernels) {
        for (auto const count : {1, 8, 11, 16, 17, 32, 257}) {
            expect_chunk_dot(kernel, count);
        }
    }
    if (has_avx512()) {
        expect_chunk_dot(
            static_cast<DotChunkKernel>(dot::strict::backend::autovec_avx512::dot_product), 257);
        expect_chunk_dot(
            static_cast<DotChunkKernel>(dot::relaxed::backend::autovec_avx512::dot_product), 257);
        expect_chunk_dot(static_cast<DotChunkKernel>(dot::relaxed::backend::avx512::dot_product),
                         257);
        expect_chunk_dot(
            static_cast<DotChunkKernel>(dot::relaxed::backend::avx512_unrolled::dot_product), 257);
    }
}

TEST(NativeSimdLab, HighwayAvx2MatchesScalarAndHandlesTailsAndPadding) {
    if ((hwy::SupportedTargets() & HWY_AVX2) == 0) {
        GTEST_SKIP() << "Highway target unavailable";
    }
    for (auto const count : {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 257}) {
        SCOPED_TRACE(count);
        expect_add(add::backend::highway_avx2::add_scaled, count);
        expect_chunk_add(static_cast<AddChunkKernel>(add::backend::highway_avx2::add_scaled),
                         count);
        expect_add(add::backend::highway_avx2_unrolled::add_scaled, count);
        expect_chunk_add(
            static_cast<AddChunkKernel>(add::backend::highway_avx2_unrolled::add_scaled), count);
        expect_dot(dot::relaxed::backend::highway_avx2::dot_product, count);
        expect_chunk_dot(
            static_cast<DotChunkKernel>(dot::relaxed::backend::highway_avx2::dot_product), count);
        expect_dot(dot::relaxed::backend::highway_avx2_unrolled::dot_product, count);
        expect_chunk_dot(
            static_cast<DotChunkKernel>(dot::relaxed::backend::highway_avx2_unrolled::dot_product),
            count);
    }
}

TEST(NativeSimdLab, HighwayAvx512MatchesScalarAndHandlesTailsAndPadding) {
    if ((hwy::SupportedTargets() & HWY_AVX3) == 0) {
        GTEST_SKIP() << "Highway target unavailable";
    }
    for (auto const count : {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 257}) {
        SCOPED_TRACE(count);
        expect_add(add::backend::highway_avx512::add_scaled, count);
        expect_chunk_add(static_cast<AddChunkKernel>(add::backend::highway_avx512::add_scaled),
                         count);
        expect_dot(dot::relaxed::backend::highway_avx512::dot_product, count);
        expect_chunk_dot(
            static_cast<DotChunkKernel>(dot::relaxed::backend::highway_avx512::dot_product), count);
        expect_dot(dot::relaxed::backend::highway_avx512_unrolled::dot_product, count);
        expect_chunk_dot(static_cast<DotChunkKernel>(
                             dot::relaxed::backend::highway_avx512_unrolled::dot_product),
                         count);
    }
}

namespace dot3 = ml::kernel_benchmark::dot_product_3d_lab;
using Dot3SoaKernel = void (*)(float const*,
                               float const*,
                               float const*,
                               float const*,
                               float const*,
                               float const*,
                               float*,
                               std::int32_t) noexcept;
using Dot3AosKernel = void (*)(dot3::Float3 const*,
                               dot3::Float3 const*,
                               float*,
                               std::int32_t) noexcept;
using Dot3ChunkKernel = void (*)(dot3::Float3Chunk16 const*,
                                 dot3::Float3Chunk16 const*,
                                 dot3::FloatChunk16*,
                                 std::int32_t) noexcept;
class HighwayDot3 : public ::testing::TestWithParam<bool> {};

TEST_P(HighwayDot3, LayoutsMatchScalarAcrossTailsAlignmentAndPadding) {
    auto const avx512{GetParam()};
    if ((hwy::SupportedTargets() & (avx512 ? HWY_AVX3 : HWY_AVX2)) == 0) {
        GTEST_SKIP() << "Highway target unavailable";
    }
    auto const soa{avx512
                       ? static_cast<Dot3SoaKernel>(dot3::backend::highway_avx512::dot_product_3d)
                       : static_cast<Dot3SoaKernel>(dot3::backend::highway_avx2::dot_product_3d)};
    auto const aos{avx512
                       ? static_cast<Dot3AosKernel>(dot3::backend::highway_avx512::dot_product_3d)
                       : static_cast<Dot3AosKernel>(dot3::backend::highway_avx2::dot_product_3d)};
    auto const chunk{
        avx512 ? static_cast<Dot3ChunkKernel>(dot3::backend::highway_avx512::dot_product_3d)
               : static_cast<Dot3ChunkKernel>(dot3::backend::highway_avx2::dot_product_3d)};
    for (auto const count : {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 257}) {
        SCOPED_TRACE(count);
        for (auto const offset : {0, 1}) {
            SCOPED_TRACE(offset);
            alignas(64) std::array<std::array<float, 272>, 6> components{};
            alignas(64) std::array<dot3::Float3, 272> lhs{};
            alignas(64) std::array<dot3::Float3, 272> rhs{};
            alignas(64) std::array<float, 272> output{};
            alignas(64) std::array<float, 272> reference{};
            for (int index{}; index < count; ++index) {
                auto const i{index + offset};
                auto const v{static_cast<float>(index) * 0.031f - 4.0f};
                components[0][i] = v;
                components[1][i] = v * -0.27f;
                components[2][i] = 1.17f - v;
                components[3][i] = v * 0.19f;
                components[4][i] = v + 2.31f;
                components[5][i] = -v;
                lhs[i] = {components[0][i], components[1][i], components[2][i]};
                rhs[i] = {components[3][i], components[4][i], components[5][i]};
            }
            dot3::backend::scalar::dot_product_3d(
                lhs.data() + offset, rhs.data() + offset, reference.data() + offset, count);
            output.fill(12345.0f);
            soa(components[0].data() + offset,
                components[1].data() + offset,
                components[2].data() + offset,
                components[3].data() + offset,
                components[4].data() + offset,
                components[5].data() + offset,
                output.data() + offset,
                count);
            for (int i{}; i < count; ++i) {
                EXPECT_EQ(output[i + offset], reference[i + offset]);
            }
            EXPECT_EQ(output[count + offset], 12345.0f);
            if (offset != 0) {
                EXPECT_EQ(output[0], 12345.0f);
            }
            output.fill(12345.0f);
            aos(lhs.data() + offset, rhs.data() + offset, output.data() + offset, count);
            for (int i{}; i < count; ++i) {
                EXPECT_EQ(output[i + offset], reference[i + offset]);
            }
            EXPECT_EQ(output[count + offset], 12345.0f);
            if (offset != 0) {
                EXPECT_EQ(output[0], 12345.0f);
            }
        }
        std::array<dot3::Float3Chunk16, 17> lhs{};
        std::array<dot3::Float3Chunk16, 17> rhs{};
        std::array<dot3::FloatChunk16, 18> output{};
        auto const chunks{(count + 15) / 16};
        for (int index{}; index < count; ++index) {
            auto const c{index / 16};
            auto const lane{index % 16};
            auto const v{static_cast<float>(index) * 0.13f};
            lhs[c].xs[lane] = v;
            lhs[c].ys[lane] = v - 2.5f;
            lhs[c].zs[lane] = -v;
            rhs[c].xs[lane] = 0.7f;
            rhs[c].ys[lane] = v * 0.31f;
            rhs[c].zs[lane] = v + 1.5f;
        }
        for (auto& block : output) {
            block.values.fill(12345.0f);
        }
        chunk(lhs.data(), rhs.data(), output.data(), chunks);
        for (int index{}; index < chunks * 16; ++index) {
            auto const c{index / 16};
            auto const lane{index % 16};
            auto const expected{
                (lhs[c].xs[lane] * rhs[c].xs[lane] + lhs[c].ys[lane] * rhs[c].ys[lane]) +
                lhs[c].zs[lane] * rhs[c].zs[lane]};
            EXPECT_EQ(output[c].values[lane], expected);
        }
        for (auto const value : output[chunks].values) {
            EXPECT_EQ(value, 12345.0f);
        }
    }
}
INSTANTIATE_TEST_SUITE_P(Target, HighwayDot3, ::testing::Bool());

}
