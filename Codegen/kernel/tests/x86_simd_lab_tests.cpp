#include "native/generated/add_scaled_x86_simd_lab.h"

#include <cpuinfo_x86.h>
#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace {

using Kernel = void (*)(float const*, float const*, float, float*, std::int32_t) noexcept;

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

}
