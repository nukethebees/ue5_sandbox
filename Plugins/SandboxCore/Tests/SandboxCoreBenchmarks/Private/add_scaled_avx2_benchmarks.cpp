#include "CoreMinimal.h"
#include "benchmark_cli_args.h"
#include "generated/add_scaled_avx2_lab.h"
#include "TestHarness.h"

#include <catch2/benchmark/catch_benchmark.hpp>

#include <string>

namespace {

struct FAlignedFloatBuffer {
    explicit FAlignedFloatBuffer(int32 const count, int32 const float_offset) {
        storage.SetNumUninitialized(count + 16);
        auto const address{reinterpret_cast<UPTRINT>(storage.GetData())};
        auto const aligned_address{(address + 31u) & ~UPTRINT{31u}};
        data = reinterpret_cast<float*>(aligned_address) + float_offset;
    }

    TArray<float> storage;
    float* data{};
};

void fill_ordinary(float* const base, float* const value, int32 const count) {
    for (int32 i{}; i < count; ++i) {
        auto const index{static_cast<float>(i)};
        base[i] = index * 0.03125f - 128.0f;
        value[i] = static_cast<float>((i * 17) % 251) * 0.125f - 16.0f;
    }
}

}

TEST_CASE("SandboxCore.GeneratedKernels.add_scaled.Avx2Lab", "[benchmark]") {
    auto const benchmark_cli_args{get_benchmark_cli_args()};
    TArray<int32> counts;
    if (benchmark_cli_args.benchmark_entities) {
        counts.Add(*benchmark_cli_args.benchmark_entities);
    } else {
        counts = {4096, 1048576};
    }

    for (auto const count : counts) {
        REQUIRE(count > 0);
        FAlignedFloatBuffer base{count, 0};
        FAlignedFloatBuffer value{count, 0};
        FAlignedFloatBuffer out{count, 0};
        fill_ordinary(base.data, value.data, count);

        auto const prefix{"ordinary/aligned/" + std::to_string(count) + "/"};
        auto benchmark_impl{[&]<auto Kernel>(std::string const& name) {
            BENCHMARK(name.c_str()) {
                Kernel(base.data, value.data, -0.75f, out.data, count);
                return out.data[count / 2];
            };
        }};

        benchmark_impl.template operator()<ml::kernel_benchmark::add_scaled_autovec_avx2>(prefix + "autovec-avx2");
        benchmark_impl.template operator()<ml::kernel_benchmark::add_scaled_avx2>(prefix + "avx2");
        benchmark_impl.template operator()<ml::kernel_benchmark::add_scaled_avx2_unrolled>(prefix + "avx2-unrolled");
    }
}
