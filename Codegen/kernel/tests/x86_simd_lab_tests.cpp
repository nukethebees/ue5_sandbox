#include "native/generated/add_scaled_x86_simd_lab.h"
#include "native/generated/dot_product_x86_simd_lab.h"

#include <gtest/gtest.h>
#include <cpuinfo_x86.h>

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
    std::vector<float> base(static_cast<std::size_t>(count));
    std::vector<float> value(static_cast<std::size_t>(count));
    std::vector<float> out(static_cast<std::size_t>(count), std::numeric_limits<float>::quiet_NaN());
    fill_values(base.data(), value.data(), count);
    kernel(base.data(), value.data(), -0.75f, out.data(), count);
    for (std::int32_t index{}; index < count; ++index) {
        EXPECT_EQ(out[index], base[index] + value[index] * -0.75f);
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
    std::vector<float> lhs(static_cast<std::size_t>(count));
    std::vector<float> rhs(static_cast<std::size_t>(count));
    fill_values(lhs.data(), rhs.data(), count);
    auto const expected{reference_dot(lhs.data(), rhs.data(), count)};
    auto const actual{kernel(lhs.data(), rhs.data(), count)};
    auto const tolerance{std::max(1.0e-5, std::abs(expected) * 2.0e-5)};
    EXPECT_NEAR(static_cast<double>(actual), expected, tolerance);
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
    std::array<AddKernel, 4> const avx2_kernels{
        add::backend::scalar::add_scaled,
        add::backend::autovec_avx2::add_scaled,
        add::backend::avx2::add_scaled,
        add::backend::avx2_unrolled::add_scaled};
    for (auto const kernel : avx2_kernels) {
        for (auto const count : {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 257}) {
            expect_add(kernel, count);
        }
    }
    if (has_avx512()) {
        std::array<AddKernel, 2> const avx512_kernels{
            add::backend::autovec_avx512::add_scaled, add::backend::avx512::add_scaled};
        for (auto const kernel : avx512_kernels) {
            for (auto const count : {0, 1, 15, 16, 17, 31, 32, 33, 257}) {
                expect_add(kernel, count);
            }
        }
    }
}

TEST(NativeSimdLab, ChunkAddProcessesEveryPhysicalLane) {
    std::array const avx2_kernels{static_cast<AddChunkKernel>(add::backend::scalar::add_scaled),
                                  static_cast<AddChunkKernel>(
                                      add::backend::autovec_avx2::add_scaled),
                                  static_cast<AddChunkKernel>(add::backend::avx2::add_scaled),
                                  static_cast<AddChunkKernel>(
                                      add::backend::avx2_unrolled::add_scaled)};
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
    std::array<DotKernel, 2> const strict_avx2{
        dot::strict::backend::scalar::dot_product,
        dot::strict::backend::autovec_avx2::dot_product};
    std::array<DotKernel, 3> const relaxed_avx2{
        dot::relaxed::backend::autovec_avx2::dot_product,
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
        expect_chunk_dot(static_cast<DotChunkKernel>(
                             dot::strict::backend::autovec_avx512::dot_product),
                         257);
        expect_chunk_dot(static_cast<DotChunkKernel>(
                             dot::relaxed::backend::autovec_avx512::dot_product),
                         257);
        expect_chunk_dot(
            static_cast<DotChunkKernel>(dot::relaxed::backend::avx512::dot_product), 257);
        expect_chunk_dot(static_cast<DotChunkKernel>(
                             dot::relaxed::backend::avx512_unrolled::dot_product),
                         257);
    }
}

}
