#include "native/generated/dot_product_3d_x86_simd_lab.h"

#include <benchmark/benchmark.h>
#include <cpuinfo_x86.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace dot = ml::kernel_benchmark::dot_product_3d_lab;

using SoaKernel = void (*)(float const*,
                           float const*,
                           float const*,
                           float const*,
                           float const*,
                           float const*,
                           float*,
                           std::int32_t) noexcept;
using AosKernel = void (*)(dot::Float3 const*, dot::Float3 const*, float*, std::int32_t) noexcept;
using ChunkKernel = void (*)(dot::Float3Chunk16 const*,
                             dot::Float3Chunk16 const*,
                             dot::FloatChunk16*,
                             std::int32_t) noexcept;

template <typename T>
class AlignedArray {
  public:
    explicit AlignedArray(std::int32_t const count)
        : count_{count}
        , data_{static_cast<T*>(::operator new[](static_cast<std::size_t>(count) * sizeof(T),
                                                 std::align_val_t{64}))} {
        std::uninitialized_value_construct_n(data_, count_);
    }

    ~AlignedArray() {
        std::destroy_n(data_, count_);
        ::operator delete[](data_, std::align_val_t{64});
    }

    AlignedArray(AlignedArray const&) = delete;
    AlignedArray& operator=(AlignedArray const&) = delete;

    auto data() -> T* { return data_; }
    auto data() const -> T const* { return data_; }
    auto operator[](std::int32_t const index) -> T& { return data_[index]; }
    auto operator[](std::int32_t const index) const -> T const& { return data_[index]; }
  private:
    std::int32_t count_;
    T* data_;
};

struct SoaBuffers {
    explicit SoaBuffers(std::int32_t const count)
        : lhs_x{count}
        , lhs_y{count}
        , lhs_z{count}
        , rhs_x{count}
        , rhs_y{count}
        , rhs_z{count}
        , out{count} {}

    AlignedArray<float> lhs_x;
    AlignedArray<float> lhs_y;
    AlignedArray<float> lhs_z;
    AlignedArray<float> rhs_x;
    AlignedArray<float> rhs_y;
    AlignedArray<float> rhs_z;
    AlignedArray<float> out;
};

struct AosBuffers {
    explicit AosBuffers(std::int32_t const count)
        : lhs{count}
        , rhs{count}
        , out{count} {}

    AlignedArray<dot::Float3> lhs;
    AlignedArray<dot::Float3> rhs;
    AlignedArray<float> out;
};

struct ChunkBuffers {
    explicit ChunkBuffers(std::int32_t const count)
        : lhs(static_cast<std::size_t>((count + dot::Float3Chunk16::capacity - 1) /
                                       dot::Float3Chunk16::capacity))
        , rhs(lhs.size())
        , out(lhs.size()) {}

    std::vector<dot::Float3Chunk16> lhs;
    std::vector<dot::Float3Chunk16> rhs;
    std::vector<dot::FloatChunk16> out;
};

auto has_avx512() -> bool {
    auto const features{cpu_features::GetX86Info().features};
    return features.avx512f && features.avx512cd && features.avx512bw && features.avx512dq &&
           features.avx512vl;
}

auto component_value(std::int32_t const index, std::int32_t const component, bool const rhs)
    -> float {
    auto const multiplier{rhs ? 29 + component * 8 : 13 + component * 6};
    auto const modulus{rhs ? 251 - component * 6 : 257 - component * 10};
    auto const scale{rhs ? 0.017f + static_cast<float>(component) * 0.006f
                         : 0.031f + static_cast<float>(component) * 0.004f};
    auto const offset{rhs ? -2.0f + static_cast<float>(component) * 0.25f
                          : -4.0f + static_cast<float>(component) * 0.5f};
    return static_cast<float>((index * multiplier) % modulus) * scale + offset;
}

void fill_buffers(SoaBuffers& buffers, std::int32_t const count) {
    for (std::int32_t index{}; index < count; ++index) {
        buffers.lhs_x[index] = component_value(index, 0, false);
        buffers.lhs_y[index] = component_value(index, 1, false);
        buffers.lhs_z[index] = component_value(index, 2, false);
        buffers.rhs_x[index] = component_value(index, 0, true);
        buffers.rhs_y[index] = component_value(index, 1, true);
        buffers.rhs_z[index] = component_value(index, 2, true);
    }
}

void fill_buffers(AosBuffers& buffers, std::int32_t const count) {
    for (std::int32_t index{}; index < count; ++index) {
        buffers.lhs[index] = {component_value(index, 0, false),
                              component_value(index, 1, false),
                              component_value(index, 2, false)};
        buffers.rhs[index] = {component_value(index, 0, true),
                              component_value(index, 1, true),
                              component_value(index, 2, true)};
    }
}

void fill_buffers(ChunkBuffers& buffers, std::int32_t const count) {
    auto const physical_count{static_cast<std::int32_t>(buffers.lhs.size()) *
                              dot::Float3Chunk16::capacity};
    for (std::int32_t index{}; index < physical_count; ++index) {
        auto const chunk_index{index / dot::Float3Chunk16::capacity};
        auto const lane{index % dot::Float3Chunk16::capacity};
        if (index < count) {
            buffers.lhs[chunk_index].xs[lane] = component_value(index, 0, false);
            buffers.lhs[chunk_index].ys[lane] = component_value(index, 1, false);
            buffers.lhs[chunk_index].zs[lane] = component_value(index, 2, false);
            buffers.rhs[chunk_index].xs[lane] = component_value(index, 0, true);
            buffers.rhs[chunk_index].ys[lane] = component_value(index, 1, true);
            buffers.rhs[chunk_index].zs[lane] = component_value(index, 2, true);
        }
    }
}

auto reference_value(std::int32_t const index) -> double {
    auto result{0.0};
    for (std::int32_t component{}; component < 3; ++component) {
        result += static_cast<double>(component_value(index, component, false)) *
                  static_cast<double>(component_value(index, component, true));
    }
    return result;
}

template <typename Output>
void record_result(benchmark::State& state, std::int32_t const count, Output const& output) {
    auto maximum_error{0.0};
    auto maximum_relative_error{0.0};
    for (std::int32_t index{}; index < count; ++index) {
        auto const expected{reference_value(index)};
        auto const error{std::abs(static_cast<double>(output(index)) - expected)};
        maximum_error = std::max(maximum_error, error);
        maximum_relative_error =
            std::max(maximum_relative_error, error / std::max(std::abs(expected), 1.0));
    }
    state.counters["maximum_absolute_error"] = maximum_error;
    state.counters["maximum_relative_error"] = maximum_relative_error;
    if (!std::isfinite(maximum_relative_error) || maximum_relative_error > 2.0e-5) {
        state.SkipWithError("3D dot-product output does not match the reference");
    }

    auto const operations{state.iterations() * static_cast<std::int64_t>(count)};
    state.SetItemsProcessed(operations);
    state.SetBytesProcessed(operations * static_cast<std::int64_t>(sizeof(float) * 7));
}

void run_soa_benchmark(benchmark::State& state,
                       SoaKernel const kernel,
                       std::int32_t const count,
                       bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError(
            "AVX-512 is unavailable or its register state is not enabled by the OS");
        return;
    }
    SoaBuffers buffers{count};
    fill_buffers(buffers, count);
    for (auto _ : state) {
        static_cast<void>(_);
        kernel(buffers.lhs_x.data(),
               buffers.lhs_y.data(),
               buffers.lhs_z.data(),
               buffers.rhs_x.data(),
               buffers.rhs_y.data(),
               buffers.rhs_z.data(),
               buffers.out.data(),
               count);
        benchmark::DoNotOptimize(buffers.out[count / 2]);
        benchmark::ClobberMemory();
    }
    record_result(state, count, [&](std::int32_t const index) { return buffers.out[index]; });
}

void run_aos_benchmark(benchmark::State& state,
                       AosKernel const kernel,
                       std::int32_t const count,
                       bool const requires_avx512) {
    if (requires_avx512 && !has_avx512()) {
        state.SkipWithError(
            "AVX-512 is unavailable or its register state is not enabled by the OS");
        return;
    }
    AosBuffers buffers{count};
    fill_buffers(buffers, count);
    for (auto _ : state) {
        static_cast<void>(_);
        kernel(buffers.lhs.data(), buffers.rhs.data(), buffers.out.data(), count);
        benchmark::DoNotOptimize(buffers.out[count / 2]);
        benchmark::ClobberMemory();
    }
    record_result(state, count, [&](std::int32_t const index) { return buffers.out[index]; });
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
    ChunkBuffers buffers{count};
    fill_buffers(buffers, count);
    auto const chunk_count{static_cast<std::int32_t>(buffers.lhs.size())};
    for (auto _ : state) {
        static_cast<void>(_);
        kernel(buffers.lhs.data(), buffers.rhs.data(), buffers.out.data(), chunk_count);
        benchmark::DoNotOptimize(buffers.out[chunk_count / 2].values[0]);
        benchmark::ClobberMemory();
    }
    record_result(state, count, [&](std::int32_t const index) {
        auto const chunk_index{index / dot::FloatChunk16::capacity};
        auto const lane{index % dot::FloatChunk16::capacity};
        return buffers.out[chunk_index].values[lane];
    });
}

struct Backend {
    std::string_view name;
    SoaKernel soa;
    AosKernel aos;
    ChunkKernel chunk;
    bool requires_avx512;
};

auto backends() -> std::array<Backend, 4> {
    return {
        Backend{"autovec-avx2",
                static_cast<SoaKernel>(dot::backend::autovec_avx2::dot_product_3d),
                static_cast<AosKernel>(dot::backend::autovec_avx2::dot_product_3d),
                static_cast<ChunkKernel>(dot::backend::autovec_avx2::dot_product_3d),
                false},
        Backend{"avx2",
                static_cast<SoaKernel>(dot::backend::avx2::dot_product_3d),
                static_cast<AosKernel>(dot::backend::avx2::dot_product_3d),
                static_cast<ChunkKernel>(dot::backend::avx2::dot_product_3d),
                false},
        Backend{"autovec-avx512",
                static_cast<SoaKernel>(dot::backend::autovec_avx512::dot_product_3d),
                static_cast<AosKernel>(dot::backend::autovec_avx512::dot_product_3d),
                static_cast<ChunkKernel>(dot::backend::autovec_avx512::dot_product_3d),
                true},
        Backend{"avx512",
                static_cast<SoaKernel>(dot::backend::avx512::dot_product_3d),
                static_cast<AosKernel>(dot::backend::avx512::dot_product_3d),
                static_cast<ChunkKernel>(dot::backend::avx512::dot_product_3d),
                true},
    };
}

auto register_benchmarks() -> bool {
    constexpr std::array counts{4096, 16384, 65536, 100000};
    for (auto const count : counts) {
        auto const name{"dot_product_3d/elementwise/aos/scalar/ordinary/aligned/" +
                        std::to_string(count)};
        benchmark::RegisterBenchmark(name.c_str(),
                                     run_aos_benchmark,
                                     static_cast<AosKernel>(dot::backend::scalar::dot_product_3d),
                                     count,
                                     false)
            ->UseRealTime();
    }
    for (auto const& backend : backends()) {
        for (auto const count : counts) {
            auto const prefix{std::string{"dot_product_3d/elementwise/"}};
            auto const suffix{"/" + std::string{backend.name} + "/ordinary/aligned/" +
                              std::to_string(count)};
            benchmark::RegisterBenchmark((prefix + "aos" + suffix).c_str(),
                                         run_aos_benchmark,
                                         backend.aos,
                                         count,
                                         backend.requires_avx512)
                ->UseRealTime();
            benchmark::RegisterBenchmark((prefix + "soa-flat" + suffix).c_str(),
                                         run_soa_benchmark,
                                         backend.soa,
                                         count,
                                         backend.requires_avx512)
                ->UseRealTime();
            benchmark::RegisterBenchmark((prefix + "soa-chunked16" + suffix).c_str(),
                                         run_chunk_benchmark,
                                         backend.chunk,
                                         count,
                                         backend.requires_avx512)
                ->UseRealTime();
        }
    }
    return true;
}

[[maybe_unused]] auto const BenchmarksRegistered{register_benchmarks()};

}
