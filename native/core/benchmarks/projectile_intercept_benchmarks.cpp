#include <sandbox/core/projectile_intercept.h>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

namespace projectile_intercept_benchmarks {
using BatchSolver = void (*)(float*,
                             ml::Vector3fSoAConstView,
                             ml::Vector3fSoAConstView,
                             ml::Vector3fSoAConstView,
                             float) noexcept;

struct ProjectileBuffers {
    explicit ProjectileBuffers(std::int32_t const count)
        : shooter_x(static_cast<std::size_t>(count), 0.0f)
        , shooter_y(static_cast<std::size_t>(count), 0.0f)
        , shooter_z(static_cast<std::size_t>(count), 0.0f)
        , target_x(static_cast<std::size_t>(count))
        , target_y(static_cast<std::size_t>(count))
        , target_z(static_cast<std::size_t>(count))
        , velocity_x(static_cast<std::size_t>(count))
        , velocity_y(static_cast<std::size_t>(count))
        , velocity_z(static_cast<std::size_t>(count))
        , out(static_cast<std::size_t>(count)) {
        for (std::int32_t index{}; index < count; ++index) {
            auto const value{static_cast<float>(index)};
            target_x[static_cast<std::size_t>(index)] = 100.0f + value;
            target_y[static_cast<std::size_t>(index)] = value * 0.25f;
            target_z[static_cast<std::size_t>(index)] = value * 0.5f;
            velocity_x[static_cast<std::size_t>(index)] = -4.0f;
            velocity_y[static_cast<std::size_t>(index)] = 1.0f;
            velocity_z[static_cast<std::size_t>(index)] = -0.5f;
        }
    }

    auto shooters() const -> ml::Vector3fSoAConstView {
        return {shooter_x.data(),
                shooter_y.data(),
                shooter_z.data(),
                static_cast<std::int32_t>(shooter_x.size())};
    }

    auto targets() const -> ml::Vector3fSoAConstView {
        return {target_x.data(),
                target_y.data(),
                target_z.data(),
                static_cast<std::int32_t>(target_x.size())};
    }

    auto velocities() const -> ml::Vector3fSoAConstView {
        return {velocity_x.data(),
                velocity_y.data(),
                velocity_z.data(),
                static_cast<std::int32_t>(velocity_x.size())};
    }

    std::vector<float> shooter_x;
    std::vector<float> shooter_y;
    std::vector<float> shooter_z;
    std::vector<float> target_x;
    std::vector<float> target_y;
    std::vector<float> target_z;
    std::vector<float> velocity_x;
    std::vector<float> velocity_y;
    std::vector<float> velocity_z;
    std::vector<float> out;
};

void run_projectile_benchmark(benchmark::State& state, BatchSolver const solver) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    ProjectileBuffers buffers{count};

    for (auto _ : state) {
        static_cast<void>(_);
        solver(
            buffers.out.data(), buffers.shooters(), buffers.targets(), buffers.velocities(), 50.0f);
        benchmark::DoNotOptimize(buffers.out[static_cast<std::size_t>(count / 2)]);
        benchmark::ClobberMemory();
    }

    auto const items{state.iterations() * static_cast<std::int64_t>(count)};
    state.SetItemsProcessed(items);
    state.SetBytesProcessed(items * static_cast<std::int64_t>(sizeof(float) * 10));
}

BENCHMARK_CAPTURE(run_projectile_benchmark,
                  aos,
                  ml::detail::solve_intercept_times_aos::solve_intercept_times)
    ->Range(1, 65536)
    ->UseRealTime();
BENCHMARK_CAPTURE(run_projectile_benchmark,
                  struct_loop,
                  ml::detail::solve_intercept_times_struct_loop::solve_intercept_times)
    ->Range(1, 65536)
    ->UseRealTime();
BENCHMARK_CAPTURE(run_projectile_benchmark,
                  soa_loop,
                  ml::detail::solve_intercept_times_soa_loop::solve_intercept_times)
    ->Range(1, 65536)
    ->UseRealTime();
}
