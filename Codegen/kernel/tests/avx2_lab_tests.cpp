#include "generated/add_scaled_avx2_lab.h"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

namespace {

constexpr int32 MaxCount{4097};
constexpr int32 Padding{16};
constexpr int32 StorageCount{MaxCount + Padding * 2};
constexpr std::uint32_t SentinelBits{0x7f123456u};

using Storage = std::array<float, StorageCount>;

auto adversarial_value(int32 const index) -> float {
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

void expect_same_bits(float const expected, float const actual, int32 const index) {
    if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(actual)) << "index " << index;
        return;
    }
    EXPECT_EQ(std::bit_cast<std::uint32_t>(actual), std::bit_cast<std::uint32_t>(expected))
        << "index " << index;
}

void run_adversarial_case(int32 const count,
                          int32 const base_offset,
                          int32 const value_offset,
                          int32 const out_offset,
                          float const scale) {
    alignas(32) Storage base{};
    alignas(32) Storage value{};
    alignas(32) Storage autovec_out{};
    alignas(32) Storage avx2_out{};
    alignas(32) Storage avx2_unrolled_out{};
    auto const sentinel{std::bit_cast<float>(SentinelBits)};
    autovec_out.fill(sentinel);
    avx2_out.fill(sentinel);
    avx2_unrolled_out.fill(sentinel);
    for (int32 index{}; index < StorageCount; ++index) {
        base[static_cast<std::size_t>(index)] = adversarial_value(index);
        value[static_cast<std::size_t>(index)] = adversarial_value(index + 5);
    }

    auto const* const base_data{base.data() + Padding + base_offset};
    auto const* const value_data{value.data() + Padding + value_offset};
    auto* const autovec_data{autovec_out.data() + Padding + out_offset};
    auto* const avx2_data{avx2_out.data() + Padding + out_offset};
    auto* const avx2_unrolled_data{avx2_unrolled_out.data() + Padding + out_offset};
    ml::kernel_benchmark::add_scaled_autovec_avx2(
        base_data, value_data, scale, autovec_data, count);
    ml::kernel_benchmark::add_scaled_avx2(base_data, value_data, scale, avx2_data, count);
    ml::kernel_benchmark::add_scaled_avx2_unrolled(
        base_data, value_data, scale, avx2_unrolled_data, count);

    for (int32 index{}; index < StorageCount; ++index) {
        expect_same_bits(autovec_out[static_cast<std::size_t>(index)],
                         avx2_out[static_cast<std::size_t>(index)],
                         index);
        expect_same_bits(autovec_out[static_cast<std::size_t>(index)],
                         avx2_unrolled_out[static_cast<std::size_t>(index)],
                         index);
    }
    EXPECT_EQ(std::bit_cast<std::uint32_t>(avx2_data[-1]), SentinelBits);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(avx2_data[count]), SentinelBits);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(avx2_unrolled_data[-1]), SentinelBits);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(avx2_unrolled_data[count]), SentinelBits);
}

TEST(KernelAvx2Lab, MatchesAutovecAcrossTailsAlignmentAndSpecialValues) {
    constexpr std::array counts{0,  1,  2,  7,   8,   9,   15,  16,  17,
                                31, 32, 33, 63,  64,  65,  127, 1024, 4097};
    constexpr std::array offsets{
        std::array{0, 0, 0}, std::array{1, 1, 1}, std::array{1, 3, 5}, std::array{7, 2, 4}};
    constexpr std::array scales{0.0f, -0.0f, 1.0f, -2.5f,
                                std::numeric_limits<float>::denorm_min(),
                                std::numeric_limits<float>::infinity()};

    for (auto const count : counts) {
        for (auto const& offset : offsets) {
            for (auto const scale : scales) {
                run_adversarial_case(count, offset[0], offset[1], offset[2], scale);
            }
        }
    }
}

TEST(KernelAvx2Lab, MatchesAutovecForDeterministicRandomFiniteValues) {
    alignas(32) Storage base{};
    alignas(32) Storage value{};
    alignas(32) Storage autovec_out{};
    alignas(32) Storage avx2_out{};
    std::mt19937 random{0x51a7c0deu};
    std::uniform_real_distribution<float> distribution{-10000.0f, 10000.0f};
    for (int32 index{}; index < StorageCount; ++index) {
        base[static_cast<std::size_t>(index)] = distribution(random);
        value[static_cast<std::size_t>(index)] = distribution(random);
    }

    for (auto const count : {7, 8, 9, 31, 32, 33, 1024, MaxCount}) {
        auto const scale{distribution(random)};
        ml::kernel_benchmark::add_scaled_autovec_avx2(
            base.data() + Padding, value.data() + Padding + 1, scale, autovec_out.data(), count);
        ml::kernel_benchmark::add_scaled_avx2(
            base.data() + Padding, value.data() + Padding + 1, scale, avx2_out.data(), count);
        for (int32 index{}; index < count; ++index) {
            expect_same_bits(autovec_out[static_cast<std::size_t>(index)],
                             avx2_out[static_cast<std::size_t>(index)],
                             index);
        }
    }
}

TEST(KernelAvx2Lab, HasAnIndependentSemanticAnchor) {
    float const base[]{1.0f, -4.0f, 0.5f};
    float const value[]{2.0f, 3.0f, -8.0f};
    float out[3]{};

    ml::kernel_benchmark::add_scaled_avx2(base, value, 3.0f, out, 3);

    EXPECT_EQ(out[0], 7.0f);
    EXPECT_EQ(out[1], 5.0f);
    EXPECT_EQ(out[2], -23.5f);
}

}
