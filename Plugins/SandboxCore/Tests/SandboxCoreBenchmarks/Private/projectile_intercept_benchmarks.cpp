#include <ioj/sim/vectors3f.h>
#include <sandbox/core/projectile_intercept.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include "CoreMinimal.h"
#include "benchmark_cli_args.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_times.Benchmarks", "[benchmark]") {
    auto const benchmark_cli_args{get_benchmark_cli_args()};
    auto const count{benchmark_cli_args.benchmark_entities.value_or(1024 * 16)};
    float constexpr projectile_speed{500.0f};

    REQUIRE(count > 0);

    ::ioj::sim::Vectors3f shooter_positions;
    ::ioj::sim::Vectors3f target_positions;
    ::ioj::sim::Vectors3f target_velocities;
    shooter_positions.set_num(count);
    target_positions.set_num(count);
    target_velocities.set_num(count);

    for (int32 i{0}; i < count; ++i) {
        auto const value{static_cast<float>(i)};
        shooter_positions.set(i, value * 0.25f, value * -0.125f, value * 0.0625f);
        target_positions.set(i, value * 1.25f + 1000.0f, value * -0.375f + 250.0f, value * 0.0625f + 100.0f);
        target_velocities.set(i, 25.0f, -10.0f + static_cast<float>(i % 20), 5.0f);
    }

    TArray<float> intercept_times;
    intercept_times.SetNumUninitialized(count);

    auto benchmark_impl{[&]<auto SolveInterceptTimes>(char const* name) {
        BENCHMARK(name) {
            return SolveInterceptTimes(intercept_times.GetData(),
                                       shooter_positions.get_const_view(),
                                       target_positions.get_const_view(),
                                       target_velocities.get_const_view(),
                                       projectile_speed);
        };
    }};

    benchmark_impl.template operator()<ml::detail::solve_intercept_times_aos::solve_intercept_times>(
        "ml::detail::solve_intercept_times_aos");

    benchmark_impl.template operator()<ml::detail::solve_intercept_times_struct_loop::solve_intercept_times>(
        "ml::detail::solve_intercept_times_struct_loop");

    benchmark_impl.template operator()<ml::detail::solve_intercept_times_soa_loop::solve_intercept_times>(
        "ml::detail::solve_intercept_times_soa_loop");

    benchmark_impl.template operator()<ml::detail::solve_intercept_times_aos::solve_intercept_times>(
        "ml::detail::solve_intercept_times_aos");

    benchmark_impl.template operator()<ml::detail::solve_intercept_times_struct_loop::solve_intercept_times>(
        "ml::detail::solve_intercept_times_struct_loop");

    benchmark_impl.template operator()<ml::detail::solve_intercept_times_soa_loop::solve_intercept_times>(
        "ml::detail::solve_intercept_times_soa_loop");
}
