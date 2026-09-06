#include "native/generated/dot_product_x86_simd_lab.h"

#include <benchmark/benchmark.h>
#include <cpuinfo_x86.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Kernel = float (*)(float const*, float const*, std::int32_t) noexcept;
using SoaosBlock = ml::kernel_benchmark::dot_product_lab::DotProductSoAoSBlock;
using SoaosKernel = float (*)(SoaosBlock const*, std::int32_t) noexcept;

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

void fill_values(float* const lhs,
                 float* const rhs,
                 std::int32_t const count,
                 std::int32_t const first_index = 0) {
    for (std::int32_t index{}; index < count; ++index) {
        auto const global_index{first_index + index};
        lhs[index] = static_cast<float>((global_index * 13) % 257) * 0.031f - 4.0f;
        rhs[index] = static_cast<float>((global_index * 29) % 251) * 0.017f - 2.0f;
    }
}

auto make_soaos_blocks(std::int32_t const count) -> std::vector<SoaosBlock> {
    auto const block_count{(count + SoaosBlock::capacity - 1) / SoaosBlock::capacity};
    std::vector<SoaosBlock> blocks(static_cast<std::size_t>(block_count));
    for (std::int32_t block_index{}; block_index < block_count; ++block_index) {
        auto& block{blocks[static_cast<std::size_t>(block_index)]};
        auto const first_index{block_index * SoaosBlock::capacity};
        auto const remaining{count - first_index};
        block.size = std::min(remaining, SoaosBlock::capacity);
        fill_values(block.lhs.data(), block.rhs.data(), block.size, first_index);
    }
    return blocks;
}

auto reference_soaos_dot_product(std::vector<SoaosBlock> const& blocks) -> double {
    double result{};
    for (auto const& block : blocks) {
        for (std::int32_t lane{}; lane < block.size; ++lane) {
            result += static_cast<double>(block.lhs[static_cast<std::size_t>(lane)]) *
                      static_cast<double>(block.rhs[static_cast<std::size_t>(lane)]);
        }
    }
    return result;
}

auto reference_dot_product(float const* const lhs,
                           float const* const rhs,
                           std::int32_t const count) -> double {
    double result{};
    for (std::int32_t index{}; index < count; ++index) {
        result += static_cast<double>(lhs[index]) * static_cast<double>(rhs[index]);
    }
    return result;
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
    auto const reference{reference_dot_product(lhs.data, rhs.data, count)};

    float result{};
    for (auto _ : state) {
        static_cast<void>(_);
        result = kernel(lhs.data, rhs.data, count);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }

    auto const operations{state.iterations() * static_cast<std::int64_t>(count)};
    state.SetItemsProcessed(operations);
    state.SetBytesProcessed(operations * static_cast<std::int64_t>(sizeof(float) * 2));
    auto const absolute_error{std::abs(static_cast<double>(result) - reference)};
    auto const relative_error{
        absolute_error / std::max(std::abs(reference), std::numeric_limits<double>::min())};
    state.counters["absolute_error"] = absolute_error;
    state.counters["relative_error"] = relative_error;
}

void run_soaos_benchmark(benchmark::State& state,
                         SoaosKernel const kernel,
                         std::int32_t const count,
                         bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError("AVX-512 is unavailable or its register state is not enabled by the OS");
        return;
    }

    auto const blocks{make_soaos_blocks(count)};
    auto const block_count{static_cast<std::int32_t>(blocks.size())};
    auto const physical_count{block_count * SoaosBlock::capacity};
    auto const reference{reference_soaos_dot_product(blocks)};

    float result{};
    for (auto _ : state) {
        static_cast<void>(_);
        result = kernel(blocks.data(), block_count);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }

    auto const iterations{state.iterations()};
    auto const logical_operations{iterations * static_cast<std::int64_t>(count)};
    auto const physical_operations{iterations * static_cast<std::int64_t>(physical_count)};
    state.SetItemsProcessed(logical_operations);
    state.SetBytesProcessed(logical_operations * static_cast<std::int64_t>(sizeof(float) * 2));
    state.counters["physical_items_per_second"] =
        benchmark::Counter(static_cast<double>(physical_operations), benchmark::Counter::kIsRate);
    state.counters["padding_fraction"] =
        static_cast<double>(physical_count - count) / static_cast<double>(physical_count);
    auto const absolute_error{std::abs(static_cast<double>(result) - reference)};
    auto const relative_error{
        absolute_error / std::max(std::abs(reference), std::numeric_limits<double>::min())};
    state.counters["absolute_error"] = absolute_error;
    state.counters["relative_error"] = relative_error;
}

struct Backend {
    std::string_view name;
    Kernel kernel;
    bool requires_avx512;
};

struct SoaosBackend {
    std::string_view name;
    SoaosKernel kernel;
    bool requires_avx512;
};

void register_case(Backend const& backend,
                   std::int32_t const count,
                   std::int32_t const offset) {
    auto const name{std::string{"dot_product/flat/"} + std::string{backend.name} +
                    "/ordinary/" + (offset == 0 ? "aligned/" : "unaligned/") +
                    std::to_string(count)};
    benchmark::RegisterBenchmark(
        name.c_str(), run_benchmark, backend.kernel, count, offset, backend.requires_avx512)
        ->UseRealTime();
}

void register_soaos_case(SoaosBackend const& backend, std::int32_t const count) {
    auto const name{std::string{"dot_product/soaos16/"} + std::string{backend.name} +
                    "/ordinary/aligned/" + std::to_string(count)};
    benchmark::RegisterBenchmark(
        name.c_str(), run_soaos_benchmark, backend.kernel, count, backend.requires_avx512)
        ->UseRealTime();
}

auto register_benchmarks() -> bool {
    namespace dot = ml::kernel_benchmark::dot_product_lab;

    auto const dispatch_backend{dot::get_dot_product_backend() == dot::X86SimdBackend::avx512
                                    ? "dispatch-avx512"
                                    : "dispatch-avx2"};
    std::array const backends{
        Backend{"scalar", dot::dot_product_scalar, false},
        Backend{"autovec-strict-avx2", dot::dot_product_autovec_strict_avx2, false},
        Backend{"autovec-relaxed-avx2", dot::dot_product_autovec_relaxed_avx2, false},
        Backend{"avx2", dot::dot_product_avx2, false},
        Backend{"avx2-unrolled", dot::dot_product_avx2_unrolled, false},
        Backend{"autovec-strict-avx512", dot::dot_product_autovec_strict_avx512, true},
        Backend{"autovec-relaxed-avx512", dot::dot_product_autovec_relaxed_avx512, true},
        Backend{"avx512", dot::dot_product_avx512, true},
        Backend{"avx512-unrolled", dot::dot_product_avx512_unrolled, true},
        Backend{dispatch_backend, dot::dot_product_dispatch, false},
    };
    std::array const soaos_backends{
        SoaosBackend{"scalar", dot::dot_product_soaos_scalar, false},
        SoaosBackend{"autovec-strict-avx2", dot::dot_product_soaos_autovec_strict_avx2, false},
        SoaosBackend{
            "autovec-relaxed-avx2", dot::dot_product_soaos_autovec_relaxed_avx2, false},
        SoaosBackend{"avx2", dot::dot_product_soaos_avx2, false},
        SoaosBackend{"avx2-unrolled", dot::dot_product_soaos_avx2_unrolled, false},
        SoaosBackend{
            "autovec-strict-avx512", dot::dot_product_soaos_autovec_strict_avx512, true},
        SoaosBackend{
            "autovec-relaxed-avx512", dot::dot_product_soaos_autovec_relaxed_avx512, true},
        SoaosBackend{"avx512", dot::dot_product_soaos_avx512, true},
        SoaosBackend{"avx512-unrolled", dot::dot_product_soaos_avx512_unrolled, true},
        SoaosBackend{dispatch_backend, dot::dot_product_soaos_dispatch, false},
    };
    constexpr std::array counts{1,   7,    8,     9,     11,     15,     16,
                                17,  31,   32,    33,    64,     256,    1024,
                                4096, 16384, 65536, 262144, 1048576};
    constexpr std::array soaos_counts{8,  11,   16,    32,     256,
                                      4096, 65536, 262144, 1048576};

    for (auto const& backend : backends) {
        for (auto const count : counts) {
            for (std::int32_t const offset : {0, 1}) {
                register_case(backend, count, offset);
            }
        }
    }
    for (auto const& backend : soaos_backends) {
        for (auto const count : soaos_counts) {
            register_soaos_case(backend, count);
        }
    }
    return true;
}

[[maybe_unused]] auto const BenchmarksRegistered{register_benchmarks()};

}
