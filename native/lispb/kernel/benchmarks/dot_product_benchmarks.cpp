#include "native/generated/dot_product_x86_simd_lab.h"

#include <benchmark/benchmark.h>
#include <cpuinfo_x86.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace dot = ml::kernel_benchmark::dot_product_lab;

using Kernel = float (*)(float const*, float const*, std::int32_t) noexcept;
using Chunk = dot::FloatChunk16;
using ChunkKernel = float (*)(Chunk const*, Chunk const*, std::int32_t) noexcept;

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
    std::vector<Chunk> lhs;
    std::vector<Chunk> rhs;
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

auto make_chunk_buffers(std::int32_t const count) -> ChunkBuffers {
    auto const chunk_count{(count + Chunk::capacity - 1) / Chunk::capacity};
    ChunkBuffers result{.lhs = std::vector<Chunk>(static_cast<std::size_t>(chunk_count)),
                        .rhs = std::vector<Chunk>(static_cast<std::size_t>(chunk_count))};
    for (std::int32_t chunk_index{}; chunk_index < chunk_count; ++chunk_index) {
        auto const first_index{chunk_index * Chunk::capacity};
        auto const remaining{std::min(count - first_index, Chunk::capacity)};
        fill_values(result.lhs[chunk_index].values.data(),
                    result.rhs[chunk_index].values.data(),
                    remaining,
                    first_index);
    }
    return result;
}

auto reference_dot_product(float const* const lhs, float const* const rhs, std::int32_t const count)
    -> double {
    double result{};
    for (std::int32_t index{}; index < count; ++index) {
        result += static_cast<double>(lhs[index]) * static_cast<double>(rhs[index]);
    }
    return result;
}

auto reference_chunk_dot_product(ChunkBuffers const& buffers, std::int32_t const count) -> double {
    auto result{0.0};
    for (std::int32_t index{}; index < count; ++index) {
        auto const chunk_index{index / Chunk::capacity};
        auto const lane{index % Chunk::capacity};
        result += static_cast<double>(buffers.lhs[chunk_index].values[lane]) *
                  static_cast<double>(buffers.rhs[chunk_index].values[lane]);
    }
    return result;
}

void record_result(benchmark::State& state,
                   std::int32_t const count,
                   float const result,
                   double const reference,
                   std::int32_t const physical_count) {
    auto const operations{state.iterations() * static_cast<std::int64_t>(count)};
    state.SetItemsProcessed(operations);
    state.SetBytesProcessed(operations * static_cast<std::int64_t>(sizeof(float) * 2));
    auto const absolute_error{std::abs(static_cast<double>(result) - reference)};
    auto const relative_error{absolute_error / std::max(std::abs(reference), 1.0)};
    state.counters["absolute_error"] = absolute_error;
    state.counters["relative_error"] = relative_error;
    if (physical_count != count) {
        auto const physical_operations{state.iterations() *
                                       static_cast<std::int64_t>(physical_count)};
        state.counters["physical_items_per_second"] = benchmark::Counter(
            static_cast<double>(physical_operations), benchmark::Counter::kIsRate);
        state.counters["padding_fraction"] =
            static_cast<double>(physical_count - count) / static_cast<double>(physical_count);
    }
}

void run_benchmark(benchmark::State& state,
                   Kernel const kernel,
                   std::int32_t const count,
                   std::int32_t const offset,
                   bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError(
            "AVX-512 is unavailable or its register state is not enabled by the OS");
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
    record_result(state, count, result, reference, count);
}

void run_chunk_benchmark(benchmark::State& state,
                         ChunkKernel const kernel,
                         std::int32_t const count,
                         bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError(
            "AVX-512 is unavailable or its register state is not enabled by the OS");
        return;
    }

    auto buffers{make_chunk_buffers(count)};
    auto const chunk_count{static_cast<std::int32_t>(buffers.lhs.size())};
    auto const physical_count{chunk_count * Chunk::capacity};
    auto const reference{reference_chunk_dot_product(buffers, count)};

    float result{};
    for (auto _ : state) {
        static_cast<void>(_);
        result = kernel(buffers.lhs.data(), buffers.rhs.data(), chunk_count);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
    record_result(state, count, result, reference, physical_count);
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

void register_case(std::string_view const policy,
                   Backend const& backend,
                   std::int32_t const count,
                   std::int32_t const offset) {
    auto const name{std::string{"dot_product/"} + std::string{policy} + "/flat/" +
                    std::string{backend.name} + "/ordinary/" +
                    (offset == 0 ? "aligned/" : "unaligned/") + std::to_string(count)};
    benchmark::RegisterBenchmark(
        name.c_str(), run_benchmark, backend.kernel, count, offset, backend.requires_avx512)
        ->UseRealTime();
}

void register_chunk_case(std::string_view const policy,
                         ChunkBackend const& backend,
                         std::int32_t const count) {
    auto const name{std::string{"dot_product/"} + std::string{policy} + "/chunked16/" +
                    std::string{backend.name} + "/ordinary/aligned/" + std::to_string(count)};
    benchmark::RegisterBenchmark(
        name.c_str(), run_chunk_benchmark, backend.kernel, count, backend.requires_avx512)
        ->UseRealTime();
}

template <std::size_t BackendCount,
          std::size_t ChunkBackendCount,
          std::size_t Count,
          std::size_t ChunkCount>
void register_policy(std::string_view const policy,
                     std::array<Backend, BackendCount> const& backends,
                     std::array<ChunkBackend, ChunkBackendCount> const& chunk_backends,
                     std::array<std::int32_t, Count> const& counts,
                     std::array<std::int32_t, ChunkCount> const& chunk_counts,
                     bool const include_unaligned) {
    for (auto const& backend : backends) {
        for (auto const count : counts) {
            register_case(policy, backend, count, 0);
            if (include_unaligned) {
                register_case(policy, backend, count, 1);
            }
        }
    }
    for (auto const& backend : chunk_backends) {
        for (auto const count : chunk_counts) {
            register_chunk_case(policy, backend, count);
        }
    }
}

auto register_benchmarks() -> bool {
    std::array const strict_backends{
        Backend{"scalar", dot::strict::backend::scalar::dot_product, false},
        Backend{"autovec-avx2", dot::strict::backend::autovec_avx2::dot_product, false},
        Backend{"autovec-avx512", dot::strict::backend::autovec_avx512::dot_product, true},
    };
    std::array const strict_chunk_backends{
        ChunkBackend{"scalar", dot::strict::backend::scalar::dot_product, false},
        ChunkBackend{"autovec-avx2", dot::strict::backend::autovec_avx2::dot_product, false},
        ChunkBackend{"autovec-avx512", dot::strict::backend::autovec_avx512::dot_product, true},
    };
    std::array const relaxed_backends{
        Backend{"autovec-avx2", dot::relaxed::backend::autovec_avx2::dot_product, false},
        Backend{"avx2", dot::relaxed::backend::avx2::dot_product, false},
        Backend{"avx2-unrolled", dot::relaxed::backend::avx2_unrolled::dot_product, false},
        Backend{"autovec-avx512", dot::relaxed::backend::autovec_avx512::dot_product, true},
        Backend{"avx512", dot::relaxed::backend::avx512::dot_product, true},
        Backend{"avx512-unrolled", dot::relaxed::backend::avx512_unrolled::dot_product, true},
    };
    std::array const relaxed_chunk_backends{
        ChunkBackend{"autovec-avx2", dot::relaxed::backend::autovec_avx2::dot_product, false},
        ChunkBackend{"avx2", dot::relaxed::backend::avx2::dot_product, false},
        ChunkBackend{"avx2-unrolled", dot::relaxed::backend::avx2_unrolled::dot_product, false},
        ChunkBackend{"autovec-avx512", dot::relaxed::backend::autovec_avx512::dot_product, true},
        ChunkBackend{"avx512", dot::relaxed::backend::avx512::dot_product, true},
        ChunkBackend{"avx512-unrolled", dot::relaxed::backend::avx512_unrolled::dot_product, true},
    };
    constexpr std::array<std::int32_t, 6> strict_counts{32, 256, 4096, 65536, 262144, 1048576};
    constexpr std::array<std::int32_t, 20> relaxed_counts{
        1,  7,  8,   9,    11,   15,    16,    17,     31,     32,
        33, 64, 256, 1024, 4096, 16384, 65536, 100000, 262144, 1048576};
    constexpr std::array<std::int32_t, 11> relaxed_chunk_counts{
        8, 11, 16, 32, 256, 4096, 16384, 65536, 100000, 262144, 1048576};

    register_policy(
        "strict", strict_backends, strict_chunk_backends, strict_counts, strict_counts, false);
    register_policy("relaxed",
                    relaxed_backends,
                    relaxed_chunk_backends,
                    relaxed_counts,
                    relaxed_chunk_counts,
                    true);
    return true;
}

[[maybe_unused]] auto const BenchmarksRegistered{register_benchmarks()};

}
