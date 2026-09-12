#include "native/generated/add_scaled_x86_simd_lab.h"

#include <benchmark/benchmark.h>
#include <cpuinfo_x86.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace add = ml::kernel_benchmark;

using Kernel = void (*)(float const*, float const*, float, float*, std::int32_t) noexcept;
using Chunk = add::FloatChunk16;
using ChunkKernel = void (*)(Chunk const*, Chunk const*, float, Chunk*, std::int32_t) noexcept;

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

struct ChunkBuffers {
    std::vector<Chunk> base;
    std::vector<Chunk> value;
    std::vector<Chunk> out;
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
    constexpr std::array values{0.0f,
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
                                std::numeric_limits<float>::quiet_NaN()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const global_index{first_index + index};
        base[index] = values[static_cast<std::size_t>(global_index) % values.size()];
        value[index] = values[static_cast<std::size_t>(global_index + 3) % values.size()];
    }
}

auto make_chunk_buffers(std::int32_t const count, bool const extreme) -> ChunkBuffers {
    auto const chunk_count{(count + Chunk::capacity - 1) / Chunk::capacity};
    ChunkBuffers result{.base = std::vector<Chunk>(static_cast<std::size_t>(chunk_count)),
                        .value = std::vector<Chunk>(static_cast<std::size_t>(chunk_count)),
                        .out = std::vector<Chunk>(static_cast<std::size_t>(chunk_count))};
    for (std::int32_t chunk_index{}; chunk_index < chunk_count; ++chunk_index) {
        auto const first_index{chunk_index * Chunk::capacity};
        if (extreme) {
            fill_extreme(result.base[chunk_index].values.data(),
                         result.value[chunk_index].values.data(),
                         Chunk::capacity,
                         first_index);
        } else {
            fill_ordinary(result.base[chunk_index].values.data(),
                          result.value[chunk_index].values.data(),
                          Chunk::capacity,
                          first_index);
        }
    }
    return result;
}

void run_benchmark(benchmark::State& state,
                   Kernel const kernel,
                   std::int32_t const count,
                   std::int32_t const offset,
                   bool const extreme,
                   bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError(
            "AVX-512 is unavailable or its register state is not enabled by the OS");
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

void run_chunk_benchmark(benchmark::State& state,
                         ChunkKernel const kernel,
                         std::int32_t const count,
                         bool const extreme,
                         bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError(
            "AVX-512 is unavailable or its register state is not enabled by the OS");
        return;
    }

    auto buffers{make_chunk_buffers(count, extreme)};
    auto const chunk_count{static_cast<std::int32_t>(buffers.base.size())};
    auto const physical_count{chunk_count * Chunk::capacity};
    auto const output_index{count / 2};
    auto const output_chunk{output_index / Chunk::capacity};
    auto const output_lane{output_index % Chunk::capacity};

    for (auto _ : state) {
        static_cast<void>(_);
        kernel(buffers.base.data(), buffers.value.data(), -0.75f, buffers.out.data(), chunk_count);
        benchmark::DoNotOptimize(buffers.out[output_chunk].values[output_lane]);
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

struct ChunkBackend {
    std::string_view name;
    ChunkKernel kernel;
    bool requires_avx512;
};

void register_case(Backend const& backend,
                   std::int32_t const count,
                   std::int32_t const offset,
                   bool const extreme) {
    auto const name{std::string{"add_scaled/elementwise/flat/"} + std::string{backend.name} + "/" +
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

void
    register_chunk_case(ChunkBackend const& backend, std::int32_t const count, bool const extreme) {
    auto const name{std::string{"add_scaled/elementwise/chunked16/"} + std::string{backend.name} +
                    "/" + (extreme ? "extreme/" : "ordinary/") + "aligned/" +
                    std::to_string(count)};
    benchmark::RegisterBenchmark(
        name.c_str(), run_chunk_benchmark, backend.kernel, count, extreme, backend.requires_avx512)
        ->UseRealTime();
}

auto register_benchmarks() -> bool {
    std::array const backends{
        Backend{"scalar", add::backend::scalar::add_scaled, false},
        Backend{"autovec-avx2", add::backend::autovec_avx2::add_scaled, false},
        Backend{"avx2", add::backend::avx2::add_scaled, false},
        Backend{"avx2-unrolled", add::backend::avx2_unrolled::add_scaled, false},
        Backend{"autovec-avx512", add::backend::autovec_avx512::add_scaled, true},
        Backend{"avx512", add::backend::avx512::add_scaled, true},
    };
    std::array const chunk_backends{
        ChunkBackend{"scalar", add::backend::scalar::add_scaled, false},
        ChunkBackend{"autovec-avx2", add::backend::autovec_avx2::add_scaled, false},
        ChunkBackend{"avx2", add::backend::avx2::add_scaled, false},
        ChunkBackend{"avx2-unrolled", add::backend::avx2_unrolled::add_scaled, false},
        ChunkBackend{"autovec-avx512", add::backend::autovec_avx512::add_scaled, true},
        ChunkBackend{"avx512", add::backend::avx512::add_scaled, true},
    };
    constexpr std::array counts{1,  7,  8,   9,    11,   15,    16,    17,     31,     32,
                                33, 64, 256, 1024, 4096, 16384, 65536, 100000, 262144, 1048576};
    constexpr std::array chunk_counts{
        8, 11, 16, 32, 256, 4096, 16384, 65536, 100000, 262144, 1048576};
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
    for (auto const& backend : chunk_backends) {
        for (auto const count : chunk_counts) {
            register_chunk_case(backend, count, false);
        }
        for (auto const count : extreme_counts) {
            register_chunk_case(backend, count, true);
        }
    }
    return true;
}

[[maybe_unused]] auto const BenchmarksRegistered{register_benchmarks()};

}
