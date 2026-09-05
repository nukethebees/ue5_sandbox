#include "CoreMinimal.h"
#include "generated/add_scaled_avx2_lab.h"
#include "TestHarness.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

auto value_for(int32 const index) -> float {
    constexpr std::array values{
        0.0f,
        -0.0f,
        1.0f,
        -1.0f,
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
    };
    return values[static_cast<std::size_t>(index) % values.size()];
}

}

TEST_CASE("SandboxCore.GeneratedKernels.add_scaled.Avx2LabCorrectness", "[unit]") {
    constexpr int32 max_count{4097};
    constexpr int32 padding{16};
    constexpr std::uint32_t sentinel_bits{0x7f123456u};
    auto const sentinel{std::bit_cast<float>(sentinel_bits)};

    TArray<float> base;
    TArray<float> value;
    TArray<float> autovec_out;
    TArray<float> avx2_out;
    TArray<float> avx2_unrolled_out;
    base.SetNumUninitialized(max_count + padding * 2);
    value.SetNumUninitialized(max_count + padding * 2);
    autovec_out.Init(sentinel, max_count + padding * 2);
    avx2_out.Init(sentinel, max_count + padding * 2);
    avx2_unrolled_out.Init(sentinel, max_count + padding * 2);
    for (int32 index{}; index < base.Num(); ++index) {
        base[index] = value_for(index);
        value[index] = value_for(index + 3);
    }

    auto const* const base_data{base.GetData() + padding + 1};
    auto const* const value_data{value.GetData() + padding + 3};
    auto* const autovec_data{autovec_out.GetData() + padding + 5};
    auto* const avx2_data{avx2_out.GetData() + padding + 7};
    auto* const avx2_unrolled_data{avx2_unrolled_out.GetData() + padding + 7};
    for (int32 const count : {0, 1, 7, 8, 9, 31, 32, 33, 4097}) {
        ml::kernel_benchmark::add_scaled_autovec_avx2(base_data, value_data, -0.75f, autovec_data, count);
        ml::kernel_benchmark::add_scaled_avx2(base_data, value_data, -0.75f, avx2_data, count);
        ml::kernel_benchmark::add_scaled_avx2_unrolled(base_data, value_data, -0.75f, avx2_unrolled_data, count);

        for (int32 index{}; index < count; ++index) {
            if (std::isnan(autovec_data[index])) {
                REQUIRE(std::isnan(avx2_data[index]));
                REQUIRE(std::isnan(avx2_unrolled_data[index]));
            } else {
                REQUIRE(std::bit_cast<std::uint32_t>(avx2_data[index]) == std::bit_cast<std::uint32_t>(autovec_data[index]));
                REQUIRE(std::bit_cast<std::uint32_t>(avx2_unrolled_data[index]) == std::bit_cast<std::uint32_t>(autovec_data[index]));
            }
        }
        REQUIRE(std::bit_cast<std::uint32_t>(avx2_data[-1]) == sentinel_bits);
        REQUIRE(std::bit_cast<std::uint32_t>(avx2_data[count]) == sentinel_bits);
        REQUIRE(std::bit_cast<std::uint32_t>(avx2_unrolled_data[-1]) == sentinel_bits);
        REQUIRE(std::bit_cast<std::uint32_t>(avx2_unrolled_data[count]) == sentinel_bits);
    }
}
