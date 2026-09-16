#include <sandbox/core/generated/array_math_kernels.h>
#include <sandbox/core/generated/vector_lerp_kernels.h>

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct Buffers {
    explicit Buffers(std::int32_t const count)
        : current_x(static_cast<std::size_t>(count))
        , current_y(static_cast<std::size_t>(count))
        , current_z(static_cast<std::size_t>(count))
        , target_x(static_cast<std::size_t>(count))
        , target_y(static_cast<std::size_t>(count))
        , target_z(static_cast<std::size_t>(count)) {
        for (std::int32_t index{}; index < count; ++index) {
            auto const value{static_cast<float>(index)};
            current_x[index] = value * .25f;
            current_y[index] = value * -.5f;
            current_z[index] = value * .75f;
            target_x[index] = value * 1.5f + 100.f;
            target_y[index] = value * -1.25f - 50.f;
            target_z[index] = value * .875f + 25.f;
        }
    }

    std::vector<float> current_x;
    std::vector<float> current_y;
    std::vector<float> current_z;
    std::vector<float> target_x;
    std::vector<float> target_y;
    std::vector<float> target_z;
};

using Kernel = void (*)(Buffers&, float) noexcept;

void run_fused(Buffers& buffers, float const alpha) noexcept {
    ml::lerp_3d_in_place(buffers.current_x,
                         buffers.current_y,
                         buffers.current_z,
                         buffers.target_x,
                         buffers.target_y,
                         buffers.target_z,
                         alpha);
}

void run_three_1d(Buffers& buffers, float const alpha) noexcept {
    ml::lerp_1d_in_place(buffers.current_x, buffers.target_x, alpha);
    ml::lerp_1d_in_place(buffers.current_y, buffers.target_y, alpha);
    ml::lerp_1d_in_place(buffers.current_z, buffers.target_z, alpha);
}

void run_benchmark(benchmark::State& state, Kernel const kernel, std::int32_t const count) {
    Buffers buffers{count};
    for (auto _ : state) {
        static_cast<void>(_);
        kernel(buffers, .125f);
        benchmark::DoNotOptimize(buffers.current_x[static_cast<std::size_t>(count / 2)]);
        benchmark::ClobberMemory();
    }

    auto const items{state.iterations() * static_cast<std::int64_t>(count)};
    state.SetItemsProcessed(items);
    state.SetBytesProcessed(items * static_cast<std::int64_t>(sizeof(float) * 6));
}

void register_case(std::string_view const name, Kernel const kernel, std::int32_t const count) {
    auto const benchmark_name{std::string{"vector_lerp/"} + std::string{name} + "/" +
                              std::to_string(count)};
    benchmark::RegisterBenchmark(benchmark_name.c_str(), run_benchmark, kernel, count)
        ->UseRealTime();
}

auto register_benchmarks() -> bool {
    for (auto const count : std::array<std::int32_t, 2>{1000, 2000}) {
        register_case("three_1d", run_three_1d, count);
        register_case("fused_3d", run_fused, count);
    }
    return true;
}

auto const benchmarks_registered{register_benchmarks()};
}
