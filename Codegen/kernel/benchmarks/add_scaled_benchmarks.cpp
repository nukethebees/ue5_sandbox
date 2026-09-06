#include "native/generated/add_scaled_x86_simd_lab.h"

#include <benchmark/benchmark.h>
#include <cpuinfo_x86.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Kernel = void (*)(float const*, float const*, float, float*, std::int32_t) noexcept;
using SoaosBlock = ml::kernel_benchmark::AddScaledSoAoSBlock;
using SoaosKernel = void (*)(SoaosBlock*, float, std::int32_t) noexcept;

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

void fill_ordinary(float* const base,
                   float* const value,
                   std::int32_t const count,
                   std::int32_t const first_index = 0) {
    for (std::int32_t index{}; index < count; ++index) {
        auto const global_index{first_index + index};
        auto const as_float{static_cast<float>(global_index)};
        base[index] = as_float * 0.03125f - 128.0f;
        value[index] = static_cast<float>((global_index * 17) % 251) * 0.125f - 16.0f;
    }
}

void fill_extreme(float* const base,
                  float* const value,
                  std::int32_t const count,
                  std::int32_t const first_index = 0) {
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
    };
    for (std::int32_t index{}; index < count; ++index) {
        auto const global_index{first_index + index};
        base[index] = values[static_cast<std::size_t>(global_index) % values.size()];
        value[index] = values[static_cast<std::size_t>(global_index + 3) % values.size()];
    }
}

auto make_soaos_blocks(std::int32_t const count, bool const extreme)
    -> std::vector<SoaosBlock> {
    auto const block_count{(count + SoaosBlock::capacity - 1) / SoaosBlock::capacity};
    std::vector<SoaosBlock> blocks(static_cast<std::size_t>(block_count));
    for (std::int32_t block_index{}; block_index < block_count; ++block_index) {
        auto& block{blocks[static_cast<std::size_t>(block_index)]};
        auto const remaining{count - block_index * SoaosBlock::capacity};
        block.size = std::min(remaining, SoaosBlock::capacity);
        auto const first_index{block_index * SoaosBlock::capacity};
        if (extreme) {
            fill_extreme(
                block.base.data(), block.value.data(), SoaosBlock::capacity, first_index);
        } else {
            fill_ordinary(
                block.base.data(), block.value.data(), SoaosBlock::capacity, first_index);
        }
    }
    return blocks;
}

void run_benchmark(benchmark::State& state,
                   Kernel const kernel,
                   std::int32_t const count,
                   std::int32_t const offset,
                   bool const extreme,
                   bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError("AVX-512 is unavailable or its register state is not enabled by the OS");
        return;
    }

    AlignedBuffer base{count, offset};
    AlignedBuffer value{count, offset};
    AlignedBuffer out{count, offset};
    if (extreme) {
        fill_extreme(base.data, value.data, count);
    } else {
        fill_ordinary(base.data, value.data, count);
    }

    for (auto _ : state) {
        static_cast<void>(_);
        kernel(base.data, value.data, -0.75f, out.data, count);
        benchmark::DoNotOptimize(out.data[count / 2]);
        benchmark::ClobberMemory();
    }

    auto const operations{state.iterations() * static_cast<std::int64_t>(count)};
    state.SetItemsProcessed(operations);
    state.SetBytesProcessed(operations * static_cast<std::int64_t>(sizeof(float) * 3));
}

void run_soaos_benchmark(benchmark::State& state,
                         SoaosKernel const kernel,
                         std::int32_t const count,
                         bool const extreme,
                         bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError("AVX-512 is unavailable or its register state is not enabled by the OS");
        return;
    }

    auto blocks{make_soaos_blocks(count, extreme)};
    auto const block_count{static_cast<std::int32_t>(blocks.size())};
    auto const physical_count{block_count * SoaosBlock::capacity};
    auto const output_index{count / 2};
    auto const output_block{output_index / SoaosBlock::capacity};
    auto const output_lane{output_index % SoaosBlock::capacity};

    for (auto _ : state) {
        static_cast<void>(_);
        kernel(blocks.data(), -0.75f, block_count);
        benchmark::DoNotOptimize(
            blocks[static_cast<std::size_t>(output_block)]
                .out[static_cast<std::size_t>(output_lane)]);
        benchmark::ClobberMemory();
    }

    auto const iterations{state.iterations()};
    auto const logical_operations{iterations * static_cast<std::int64_t>(count)};
    auto const physical_operations{iterations * static_cast<std::int64_t>(physical_count)};
    state.SetItemsProcessed(logical_operations);
    state.SetBytesProcessed(logical_operations * static_cast<std::int64_t>(sizeof(float) * 3));
    state.counters["physical_items_per_second"] =
        benchmark::Counter(static_cast<double>(physical_operations), benchmark::Counter::kIsRate);
    state.counters["padding_fraction"] =
        static_cast<double>(physical_count - count) / static_cast<double>(physical_count);
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
                   std::int32_t const offset,
                   bool const extreme) {
    auto const name{std::string{"add_scaled/flat/"} + std::string{backend.name} + "/" +
                    (extreme ? "extreme/" : "ordinary/") +
                    (offset == 0 ? "aligned/" : "unaligned/") + std::to_string(count)};
    benchmark::RegisterBenchmark(name.c_str(),
                                 run_benchmark,
                                 backend.kernel,
                                 count,
                                 offset,
                                 extreme,
                                 backend.requires_avx512)
        ->UseRealTime();
}

void register_soaos_case(SoaosBackend const& backend,
                         std::int32_t const count,
                         bool const extreme) {
    auto const name{std::string{"add_scaled/soaos16/"} + std::string{backend.name} + "/" +
                    (extreme ? "extreme/" : "ordinary/") + "aligned/" +
                    std::to_string(count)};
    benchmark::RegisterBenchmark(name.c_str(),
                                 run_soaos_benchmark,
                                 backend.kernel,
                                 count,
                                 extreme,
                                 backend.requires_avx512)
        ->UseRealTime();
}

auto register_benchmarks() -> bool {
    auto const dispatch_backend{ml::kernel_benchmark::get_add_scaled_backend() ==
                                        ml::kernel_benchmark::X86SimdBackend::avx512
                                    ? "dispatch-avx512"
                                    : "dispatch-avx2"};
    std::array const backends{
        Backend{"scalar", ml::kernel_benchmark::add_scaled_scalar, false},
        Backend{"autovec-avx2", ml::kernel_benchmark::add_scaled_autovec_avx2, false},
        Backend{"avx2", ml::kernel_benchmark::add_scaled_avx2, false},
        Backend{"avx2-unrolled", ml::kernel_benchmark::add_scaled_avx2_unrolled, false},
        Backend{"autovec-avx512", ml::kernel_benchmark::add_scaled_autovec_avx512, true},
        Backend{"avx512", ml::kernel_benchmark::add_scaled_avx512, true},
        Backend{dispatch_backend, ml::kernel_benchmark::add_scaled_dispatch, false},
    };
    std::array const soaos_backends{
        SoaosBackend{"scalar", ml::kernel_benchmark::add_scaled_soaos_scalar, false},
        SoaosBackend{"autovec-avx2", ml::kernel_benchmark::add_scaled_soaos_autovec_avx2, false},
        SoaosBackend{"avx2", ml::kernel_benchmark::add_scaled_soaos_avx2, false},
        SoaosBackend{"avx2-unrolled", ml::kernel_benchmark::add_scaled_soaos_avx2_unrolled, false},
        SoaosBackend{
            "autovec-avx512", ml::kernel_benchmark::add_scaled_soaos_autovec_avx512, true},
        SoaosBackend{"avx512", ml::kernel_benchmark::add_scaled_soaos_avx512, true},
        SoaosBackend{dispatch_backend, ml::kernel_benchmark::add_scaled_soaos_dispatch, false},
    };
    constexpr std::array counts{1,   7,    8,     9,     11,     15,     16,
                                17,  31,   32,    33,    64,     256,    1024,
                                4096, 16384, 65536, 262144, 1048576};
    constexpr std::array soaos_counts{8,  11,   16,    32,     256,
                                      4096, 65536, 262144, 1048576};
    constexpr std::array extreme_counts{32, 256, 4096, 65536, 1048576};

    for (auto const& backend : backends) {
        for (auto const count : counts) {
            for (std::int32_t const offset : {0, 1}) {
                register_case(backend, count, offset, false);
            }
        }
        for (auto const count : extreme_counts) {
            for (std::int32_t const offset : {0, 1}) {
                register_case(backend, count, offset, true);
            }
        }
    }
    for (auto const& backend : soaos_backends) {
        for (auto const count : soaos_counts) {
            register_soaos_case(backend, count, false);
        }
        for (auto const count : extreme_counts) {
            register_soaos_case(backend, count, true);
        }
    }
    return true;
}

[[maybe_unused]] auto const BenchmarksRegistered{register_benchmarks()};

}
