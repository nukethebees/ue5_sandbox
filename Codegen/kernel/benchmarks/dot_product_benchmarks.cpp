#include "native/generated/dot_product_x86_simd_lab.h"

#include <benchmark/benchmark.h>
#include <cpuinfo_x86.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Kernel = float (*)(float const*, float const*, std::int32_t) noexcept;

struct AlignedBuffer {
    explicit AlignedBuffer(std::int32_t const count, std::int32_t const offset)
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

void fill_values(float* const lhs, float* const rhs, std::int32_t const count) {
    for (std::int32_t index{}; index < count; ++index) {
        lhs[index] = static_cast<float>((index * 13) % 257) * 0.03125f - 4.0f;
        rhs[index] = static_cast<float>((index * 29) % 251) * 0.015625f - 2.0f;
    }
}

void run_benchmark(benchmark::State& state,
                   Kernel const kernel,
                   std::int32_t const count,
                   std::int32_t const offset,
                   bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError("AVX-512 is unavailable or its register state is not enabled by the OS");
        return;
    }

    AlignedBuffer lhs{count, offset};
    AlignedBuffer rhs{count, offset};
    fill_values(lhs.data, rhs.data, count);

    for (auto _ : state) {
        static_cast<void>(_);
        auto result{kernel(lhs.data, rhs.data, count)};
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }

    auto const operations{state.iterations() * static_cast<std::int64_t>(count)};
    state.SetItemsProcessed(operations);
    state.SetBytesProcessed(operations * static_cast<std::int64_t>(sizeof(float) * 2));
}

struct Backend {
    std::string_view name;
    Kernel kernel;
    bool requires_avx512;
};

void register_case(Backend const& backend,
                   std::int32_t const count,
                   std::int32_t const offset) {
    auto const name{std::string{"dot_product/"} + std::string{backend.name} +
                    "/ordinary/" + (offset == 0 ? "aligned/" : "unaligned/") +
                    std::to_string(count)};
    benchmark::RegisterBenchmark(
        name.c_str(), run_benchmark, backend.kernel, count, offset, backend.requires_avx512)
        ->UseRealTime();
}

auto register_benchmarks() -> bool {
    namespace dot = ml::kernel_benchmark::dot_product_lab;

    auto const dispatch_backend{dot::get_dot_product_backend() == dot::X86SimdBackend::avx512
                                    ? "dispatch-avx512"
                                    : "dispatch-avx2"};
    std::array const backends{
        Backend{"scalar", dot::dot_product_scalar, false},
        Backend{"autovec-avx2", dot::dot_product_autovec_avx2, false},
        Backend{"avx2", dot::dot_product_avx2, false},
        Backend{"avx2-unrolled", dot::dot_product_avx2_unrolled, false},
        Backend{"autovec-avx512", dot::dot_product_autovec_avx512, true},
        Backend{"avx512", dot::dot_product_avx512, true},
        Backend{dispatch_backend, dot::dot_product_dispatch, false},
    };
    constexpr std::array counts{1,    7,     8,     9,      15,     16,
                                17,   31,    32,    33,     64,     256,
                                1024, 4096,  16384, 65536,  262144, 1048576};

    for (auto const& backend : backends) {
        for (auto const count : counts) {
            for (std::int32_t const offset : {0, 1}) {
                register_case(backend, count, offset);
            }
        }
    }
    return true;
}

[[maybe_unused]] auto const BenchmarksRegistered{register_benchmarks()};

}
