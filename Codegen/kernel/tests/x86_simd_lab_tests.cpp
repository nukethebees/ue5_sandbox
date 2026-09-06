#include "native/generated/add_scaled_x86_simd_lab.h"
#include "native/generated/dot_product_x86_simd_lab.h"

#include <cpuinfo_x86.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace {

using Kernel = void (*)(float const*, float const*, float, float*, std::int32_t) noexcept;
using DotKernel = float (*)(float const*, float const*, std::int32_t) noexcept;
using SoaosBlock = ml::kernel_benchmark::AddScaledSoAoSBlock;
using SoaosKernel = void (*)(SoaosBlock*, float, std::int32_t) noexcept;
using DotSoaosBlock = ml::kernel_benchmark::dot_product_lab::DotProductSoAoSBlock;
using DotSoaosKernel = float (*)(DotSoaosBlock const*, std::int32_t) noexcept;

struct AlignedBuffer {
    explicit AlignedBuffer(std::int32_t const count, std::int32_t const offset = 0)
        : storage(static_cast<std::size_t>(count) + 32) {
        auto const address{reinterpret_cast<std::uintptr_t>(storage.data())};
        auto const aligned_address{(address + 63u) & ~std::uintptr_t{63u}};
        data = reinterpret_cast<float*>(aligned_address) + offset;
    }

    std::vector<float> storage;
    float* data{};
};

auto has_avx512() -> bool {
    auto const features{cpu_features::GetX86Info().features};
    return features.avx512f && features.avx512cd && features.avx512bw && features.avx512dq &&
           features.avx512vl;
}

auto adversarial_value(std::int32_t const index) -> float {
    constexpr std::array values{
        0.0f,
        -0.0f,
        1.0f,
        -1.0f,
        std::numeric_limits<float>::min(),
        -std::numeric_limits<float>::min(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::denorm_min(),
        -std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
        0.25f,
        -8.5f,
    };
    return values[static_cast<std::size_t>(index) % values.size()];
}

void expect_same(float const expected, float const actual, std::int32_t const index) {
    if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(actual)) << "index " << index;
        return;
    }
    EXPECT_EQ(std::bit_cast<std::uint32_t>(actual), std::bit_cast<std::uint32_t>(expected))
        << "index " << index;
}

void run_case(Kernel const kernel,
              std::string_view const name,
              std::int32_t const count,
              std::int32_t const offset,
              float const scale) {
    constexpr std::uint32_t SentinelBits{0x7f123456u};
    auto const sentinel{std::bit_cast<float>(SentinelBits)};
    AlignedBuffer base{count + 2, offset};
    AlignedBuffer value{count + 2, offset};
    AlignedBuffer reference{count + 2, offset};
    AlignedBuffer result{count + 2, offset};
    for (std::int32_t index{}; index < count + 2; ++index) {
        base.data[index] = adversarial_value(index);
        value.data[index] = adversarial_value(index + 5);
        reference.data[index] = sentinel;
        result.data[index] = sentinel;
    }

    ml::kernel_benchmark::add_scaled_autovec_avx2(
        base.data, value.data, scale, reference.data + 1, count);
    kernel(base.data, value.data, scale, result.data + 1, count);

    SCOPED_TRACE(name);
    for (std::int32_t index{}; index < count; ++index) {
        expect_same(reference.data[index + 1], result.data[index + 1], index);
    }
    EXPECT_EQ(std::bit_cast<std::uint32_t>(result.data[0]), SentinelBits);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(result.data[count + 1]), SentinelBits);
}

TEST(KernelNativeSimdLab, EveryBackendMatchesAcrossTailsAlignmentAndSpecialValues) {
    struct Backend {
        std::string_view name;
        Kernel kernel;
        bool requires_avx512;
    };
    auto const avx512_available{has_avx512()};
    std::array const backends{
        Backend{"scalar", ml::kernel_benchmark::add_scaled_scalar, false},
        Backend{"autovec-avx2", ml::kernel_benchmark::add_scaled_autovec_avx2, false},
        Backend{"avx2", ml::kernel_benchmark::add_scaled_avx2, false},
        Backend{"avx2-unrolled", ml::kernel_benchmark::add_scaled_avx2_unrolled, false},
        Backend{"autovec-avx512", ml::kernel_benchmark::add_scaled_autovec_avx512, true},
        Backend{"avx512", ml::kernel_benchmark::add_scaled_avx512, true},
        Backend{"dispatch", ml::kernel_benchmark::add_scaled_dispatch, false},
    };
    constexpr std::array counts{0,  1,  2,  7,  8,  9,  15, 16, 17,
                                31, 32, 33, 63, 64, 65, 127, 257, 1025};
    constexpr std::array scales{0.0f,
                                -0.0f,
                                1.0f,
                                -2.5f,
                                std::numeric_limits<float>::denorm_min(),
                                std::numeric_limits<float>::infinity()};

    for (auto const& backend : backends) {
        if (backend.requires_avx512 && !avx512_available) {
            continue;
        }
        for (auto const count : counts) {
            for (std::int32_t const offset : {0, 1}) {
                for (auto const scale : scales) {
                    run_case(backend.kernel, backend.name, count, offset, scale);
                }
            }
        }
    }
}

void run_soaos_case(SoaosKernel const kernel,
                    std::string_view const name,
                    std::int32_t const count,
                    float const scale) {
    constexpr std::uint32_t SentinelBits{0x7f123456u};
    auto const sentinel{std::bit_cast<float>(SentinelBits)};
    auto const block_count{(count + SoaosBlock::capacity - 1) / SoaosBlock::capacity};
    std::vector<SoaosBlock> blocks(static_cast<std::size_t>(block_count));
    for (std::int32_t block_index{}; block_index < block_count; ++block_index) {
        auto& block{blocks[static_cast<std::size_t>(block_index)]};
        auto const remaining{count - block_index * SoaosBlock::capacity};
        block.size = std::min(remaining, SoaosBlock::capacity);
        for (std::int32_t lane{}; lane < SoaosBlock::capacity; ++lane) {
            auto const index{block_index * SoaosBlock::capacity + lane};
            block.base[static_cast<std::size_t>(lane)] = adversarial_value(index);
            block.value[static_cast<std::size_t>(lane)] = adversarial_value(index + 5);
            block.out[static_cast<std::size_t>(lane)] = sentinel;
        }
    }

    kernel(blocks.data(), scale, block_count);

    SCOPED_TRACE(name);
    for (std::int32_t block_index{}; block_index < block_count; ++block_index) {
        auto const& block{blocks[static_cast<std::size_t>(block_index)]};
        auto const expected_size{
            std::min(count - block_index * SoaosBlock::capacity, SoaosBlock::capacity)};
        EXPECT_EQ(block.size, expected_size);
        for (std::int32_t lane{}; lane < SoaosBlock::capacity; ++lane) {
            auto const index{block_index * SoaosBlock::capacity + lane};
            auto const expected{adversarial_value(index) + adversarial_value(index + 5) * scale};
            expect_same(expected, block.out[static_cast<std::size_t>(lane)], index);
        }
    }
}

TEST(KernelNativeSimdLab, SoaosBackendsProcessEveryPhysicalLaneAndPreserveSizes) {
    struct Backend {
        std::string_view name;
        SoaosKernel kernel;
        bool requires_avx512;
    };
    auto const avx512_available{has_avx512()};
    std::array const backends{
        Backend{"scalar", ml::kernel_benchmark::add_scaled_soaos_scalar, false},
        Backend{"autovec-avx2", ml::kernel_benchmark::add_scaled_soaos_autovec_avx2, false},
        Backend{"avx2", ml::kernel_benchmark::add_scaled_soaos_avx2, false},
        Backend{"avx2-unrolled", ml::kernel_benchmark::add_scaled_soaos_avx2_unrolled, false},
        Backend{"autovec-avx512", ml::kernel_benchmark::add_scaled_soaos_autovec_avx512, true},
        Backend{"avx512", ml::kernel_benchmark::add_scaled_soaos_avx512, true},
        Backend{"dispatch", ml::kernel_benchmark::add_scaled_soaos_dispatch, false},
    };
    constexpr std::array counts{0, 1, 7, 8, 15, 16, 17, 31, 32, 33, 65, 257};
    constexpr std::array scales{0.0f,
                                -0.0f,
                                1.0f,
                                -2.5f,
                                std::numeric_limits<float>::denorm_min(),
                                std::numeric_limits<float>::infinity()};

    for (auto const& backend : backends) {
        if (backend.requires_avx512 && !avx512_available) {
            continue;
        }
        for (auto const count : counts) {
            for (auto const scale : scales) {
                run_soaos_case(backend.kernel, backend.name, count, scale);
            }
        }
    }
}

TEST(KernelNativeSimdLab, DispatchReportsTheDetectedBackend) {
    auto const expected{has_avx512() ? ml::kernel_benchmark::X86SimdBackend::avx512
                                    : ml::kernel_benchmark::X86SimdBackend::avx2};
    EXPECT_EQ(ml::kernel_benchmark::get_add_scaled_backend(), expected);
}

TEST(KernelNativeSimdLab, HasAnIndependentSemanticAnchor) {
    float const base[]{1.0f, -4.0f, 0.5f};
    float const value[]{2.0f, 3.0f, -8.0f};
    float out[3]{};

    ml::kernel_benchmark::add_scaled_dispatch(base, value, 3.0f, out, 3);

    EXPECT_EQ(out[0], 7.0f);
    EXPECT_EQ(out[1], 5.0f);
    EXPECT_EQ(out[2], -23.5f);
}

void fill_dot_values(float* const lhs, float* const rhs, std::int32_t const count) {
    for (std::int32_t index{}; index < count; ++index) {
        lhs[index] = static_cast<float>((index * 13) % 127 - 63) * 0.007f;
        rhs[index] = static_cast<float>((index * 29) % 113 - 56) * 0.015f;
    }
}

struct DotReference {
    double value;
    double absolute_sum;
};

struct DotError {
    double absolute;
    double relative;
};

struct DotBackend {
    std::string_view name;
    DotKernel kernel;
    bool requires_avx512;
};

auto dot_backends() -> std::array<DotBackend, 10> {
    namespace dot = ml::kernel_benchmark::dot_product_lab;

    return {
        DotBackend{"scalar", dot::dot_product_scalar, false},
        DotBackend{"autovec-strict-avx2", dot::dot_product_autovec_strict_avx2, false},
        DotBackend{"autovec-relaxed-avx2", dot::dot_product_autovec_relaxed_avx2, false},
        DotBackend{"avx2", dot::dot_product_avx2, false},
        DotBackend{"avx2-unrolled", dot::dot_product_avx2_unrolled, false},
        DotBackend{"autovec-strict-avx512", dot::dot_product_autovec_strict_avx512, true},
        DotBackend{"autovec-relaxed-avx512", dot::dot_product_autovec_relaxed_avx512, true},
        DotBackend{"avx512", dot::dot_product_avx512, true},
        DotBackend{"avx512-unrolled", dot::dot_product_avx512_unrolled, true},
        DotBackend{"dispatch", dot::dot_product_dispatch, false},
    };
}

struct DotSoaosBackend {
    std::string_view name;
    DotSoaosKernel kernel;
    bool requires_avx512;
};

auto dot_soaos_backends() -> std::array<DotSoaosBackend, 10> {
    namespace dot = ml::kernel_benchmark::dot_product_lab;

    return {
        DotSoaosBackend{"scalar", dot::dot_product_soaos_scalar, false},
        DotSoaosBackend{"autovec-strict-avx2", dot::dot_product_soaos_autovec_strict_avx2, false},
        DotSoaosBackend{
            "autovec-relaxed-avx2", dot::dot_product_soaos_autovec_relaxed_avx2, false},
        DotSoaosBackend{"avx2", dot::dot_product_soaos_avx2, false},
        DotSoaosBackend{"avx2-unrolled", dot::dot_product_soaos_avx2_unrolled, false},
        DotSoaosBackend{
            "autovec-strict-avx512", dot::dot_product_soaos_autovec_strict_avx512, true},
        DotSoaosBackend{
            "autovec-relaxed-avx512", dot::dot_product_soaos_autovec_relaxed_avx512, true},
        DotSoaosBackend{"avx512", dot::dot_product_soaos_avx512, true},
        DotSoaosBackend{"avx512-unrolled", dot::dot_product_soaos_avx512_unrolled, true},
        DotSoaosBackend{"dispatch", dot::dot_product_soaos_dispatch, false},
    };
}

auto dot_reference(float const* const lhs,
                   float const* const rhs,
                   std::int32_t const count) -> DotReference {
    double result{};
    double absolute_sum{};
    for (std::int32_t index{}; index < count; ++index) {
        auto const product{static_cast<double>(lhs[index]) * static_cast<double>(rhs[index])};
        result += product;
        absolute_sum += std::abs(product);
    }
    return {result, absolute_sum};
}

auto dot_error(float const actual, DotReference const& reference) -> DotError {
    auto const absolute{std::abs(static_cast<double>(actual) - reference.value)};
    auto const relative{absolute /
                        std::max(std::abs(reference.value),
                                 std::numeric_limits<double>::min())};
    return {absolute, relative};
}

auto expect_dot_result(DotKernel const kernel,
                       std::string_view const name,
                       float const* const lhs,
                       float const* const rhs,
                       std::int32_t const count) -> DotError {
    auto const reference{dot_reference(lhs, rhs, count)};
    auto const actual{kernel(lhs, rhs, count)};
    auto const tolerance{std::max(1.0e-6,
                                  reference.absolute_sum *
                                      static_cast<double>(std::numeric_limits<float>::epsilon()) *
                                      static_cast<double>(count + 1))};

    SCOPED_TRACE(name);
    EXPECT_NEAR(static_cast<double>(actual), reference.value, tolerance);
    return dot_error(actual, reference);
}

void run_dot_case(DotKernel const kernel,
                  std::string_view const name,
                  std::int32_t const count,
                  std::int32_t const offset) {
    AlignedBuffer lhs{count, offset};
    AlignedBuffer rhs{count, offset};
    fill_dot_values(lhs.data, rhs.data, count);
    static_cast<void>(expect_dot_result(kernel, name, lhs.data, rhs.data, count));
}

TEST(KernelNativeDotProductLab, EveryBackendMatchesAHighPrecisionReference) {
    auto const avx512_available{has_avx512()};
    auto const backends{dot_backends()};
    constexpr std::array counts{0,  1,  2,  7,   8,   9,   15, 16, 17,
                                31, 32, 33, 63, 64, 65, 127, 257, 4097};

    for (auto const& backend : backends) {
        if (backend.requires_avx512 && !avx512_available) {
            continue;
        }
        for (auto const count : counts) {
            for (std::int32_t const offset : {0, 1}) {
                run_dot_case(backend.kernel, backend.name, count, offset);
            }
        }
    }
}

void fill_cancellation_values(float* const lhs, float* const rhs, std::int32_t const count) {
    for (std::int32_t index{}; index < count; ++index) {
        constexpr std::array values{100'000'000.0f, 1.0f, -100'000'000.0f, 1.0f};
        lhs[index] = values[static_cast<std::size_t>(index) % values.size()];
        rhs[index] = 1.0f;
    }
}

void fill_random_mixed_sign_values(float* const lhs,
                                   float* const rhs,
                                   std::int32_t const count) {
    std::uint32_t state{0x6d2b79f5u};
    auto next_value = [&state]() -> float {
        state = state * 1'664'525u + 1'013'904'223u;
        auto const centered{static_cast<std::int32_t>((state >> 8u) % 2001u) - 1000};
        return static_cast<float>(centered) * 0.001f;
    };
    for (std::int32_t index{}; index < count; ++index) {
        lhs[index] = next_value();
        rhs[index] = next_value();
    }
}

using DotValueFiller = void (*)(float*, float*, std::int32_t);

auto make_dot_soaos_blocks(std::int32_t const count, DotValueFiller const fill)
    -> std::vector<DotSoaosBlock> {
    AlignedBuffer lhs{count};
    AlignedBuffer rhs{count};
    fill(lhs.data, rhs.data, count);

    auto const block_count{(count + DotSoaosBlock::capacity - 1) / DotSoaosBlock::capacity};
    std::vector<DotSoaosBlock> blocks(static_cast<std::size_t>(block_count));
    for (std::int32_t block_index{}; block_index < block_count; ++block_index) {
        auto& block{blocks[static_cast<std::size_t>(block_index)]};
        auto const first_index{block_index * DotSoaosBlock::capacity};
        block.size = std::min(count - first_index, DotSoaosBlock::capacity);
        for (std::int32_t lane{}; lane < block.size; ++lane) {
            auto const index{first_index + lane};
            block.lhs[static_cast<std::size_t>(lane)] = lhs.data[index];
            block.rhs[static_cast<std::size_t>(lane)] = rhs.data[index];
        }
    }
    return blocks;
}

auto dot_soaos_reference(std::vector<DotSoaosBlock> const& blocks) -> DotReference {
    double result{};
    double absolute_sum{};
    for (auto const& block : blocks) {
        for (std::int32_t lane{}; lane < block.size; ++lane) {
            auto const product{
                static_cast<double>(block.lhs[static_cast<std::size_t>(lane)]) *
                static_cast<double>(block.rhs[static_cast<std::size_t>(lane)])};
            result += product;
            absolute_sum += std::abs(product);
        }
    }
    return {result, absolute_sum};
}

auto expect_dot_soaos_result(DotSoaosKernel const kernel,
                             std::string_view const name,
                             std::vector<DotSoaosBlock> const& blocks,
                             std::int32_t const logical_count) -> DotError {
    auto const reference{dot_soaos_reference(blocks)};
    auto const actual{kernel(blocks.data(), static_cast<std::int32_t>(blocks.size()))};
    auto const tolerance{
        std::max(1.0e-6,
                 reference.absolute_sum *
                     static_cast<double>(std::numeric_limits<float>::epsilon()) *
                     static_cast<double>(logical_count + 1))};

    SCOPED_TRACE(name);
    EXPECT_NEAR(static_cast<double>(actual), reference.value, tolerance);
    for (std::size_t block_index{}; block_index < blocks.size(); ++block_index) {
        auto const expected_size{std::min(logical_count -
                                              static_cast<std::int32_t>(block_index) *
                                                  DotSoaosBlock::capacity,
                                          DotSoaosBlock::capacity)};
        EXPECT_EQ(blocks[block_index].size, expected_size);
        for (std::int32_t lane{blocks[block_index].size}; lane < DotSoaosBlock::capacity; ++lane) {
            EXPECT_EQ(blocks[block_index].lhs[static_cast<std::size_t>(lane)], 0.0f);
            EXPECT_EQ(blocks[block_index].rhs[static_cast<std::size_t>(lane)], 0.0f);
        }
    }
    return dot_error(actual, reference);
}

TEST(KernelNativeDotProductLab, SoaosBackendsUseNeutralPaddingAcrossBlockBoundaries) {
    auto const avx512_available{has_avx512()};
    auto const backends{dot_soaos_backends()};
    constexpr std::array counts{0, 1, 7, 8, 15, 16, 17, 31, 32, 33, 65, 257, 4097};

    for (auto const& backend : backends) {
        if (backend.requires_avx512 && !avx512_available) {
            continue;
        }
        for (auto const count : counts) {
            auto const blocks{make_dot_soaos_blocks(count, fill_dot_values)};
            static_cast<void>(
                expect_dot_soaos_result(backend.kernel, backend.name, blocks, count));
        }
    }
}

TEST(KernelNativeDotProductLab, SoaosBackendsBoundCancellationAndRandomizedError) {
    auto const avx512_available{has_avx512()};
    auto const backends{dot_soaos_backends()};
    constexpr std::array counts{17, 257, 4097};

    for (auto const& backend : backends) {
        if (backend.requires_avx512 && !avx512_available) {
            continue;
        }

        DotError maximum_error{};
        for (auto const count : counts) {
            for (auto const fill : {fill_cancellation_values, fill_random_mixed_sign_values}) {
                auto const blocks{make_dot_soaos_blocks(count, fill)};
                auto const error{
                    expect_dot_soaos_result(backend.kernel, backend.name, blocks, count)};
                maximum_error.absolute = std::max(maximum_error.absolute, error.absolute);
                maximum_error.relative = std::max(maximum_error.relative, error.relative);
            }
        }

        auto property_name{"soaos_" + std::string{backend.name}};
        std::ranges::replace(property_name, '-', '_');
        RecordProperty(property_name + "_max_absolute_error", maximum_error.absolute);
        RecordProperty(property_name + "_max_relative_error", maximum_error.relative);
    }
}

TEST(KernelNativeDotProductLab, BoundsAndRecordsCancellationAndRandomizedError) {
    auto const avx512_available{has_avx512()};
    auto const backends{dot_backends()};
    constexpr std::array counts{17, 257, 4097};

    for (auto const& backend : backends) {
        if (backend.requires_avx512 && !avx512_available) {
            continue;
        }

        DotError maximum_error{};
        for (auto const count : counts) {
            for (std::int32_t const offset : {0, 1}) {
                AlignedBuffer lhs{count, offset};
                AlignedBuffer rhs{count, offset};

                fill_cancellation_values(lhs.data, rhs.data, count);
                auto const cancellation_error{
                    expect_dot_result(backend.kernel, backend.name, lhs.data, rhs.data, count)};
                maximum_error.absolute =
                    std::max(maximum_error.absolute, cancellation_error.absolute);
                maximum_error.relative =
                    std::max(maximum_error.relative, cancellation_error.relative);

                fill_random_mixed_sign_values(lhs.data, rhs.data, count);
                auto const random_error{
                    expect_dot_result(backend.kernel, backend.name, lhs.data, rhs.data, count)};
                maximum_error.absolute = std::max(maximum_error.absolute, random_error.absolute);
                maximum_error.relative = std::max(maximum_error.relative, random_error.relative);
            }
        }

        auto property_name{std::string{backend.name}};
        std::ranges::replace(property_name, '-', '_');
        RecordProperty(property_name + "_max_absolute_error", maximum_error.absolute);
        RecordProperty(property_name + "_max_relative_error", maximum_error.relative);
    }
}

TEST(KernelNativeDotProductLab, DispatchReportsTheDetectedBackend) {
    namespace dot = ml::kernel_benchmark::dot_product_lab;

    auto const expected{has_avx512() ? dot::X86SimdBackend::avx512
                                    : dot::X86SimdBackend::avx2};
    EXPECT_EQ(dot::get_dot_product_backend(), expected);
}

TEST(KernelNativeDotProductLab, HasAnIndependentSemanticAnchor) {
    namespace dot = ml::kernel_benchmark::dot_product_lab;

    float const lhs[]{1.0f, 2.0f, 3.0f};
    float const rhs[]{4.0f, 5.0f, 6.0f};

    EXPECT_EQ(dot::dot_product_dispatch(lhs, rhs, 3), 32.0f);
}

}
