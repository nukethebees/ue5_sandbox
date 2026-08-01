#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_vector_utils.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_times.Benchmarks", "[benchmark]") {
    int32 constexpr count{16 * 1024};
    float constexpr projectile_speed{500.0f};

    TArray<FVector3f> shooter_positions_aos;
    TArray<FVector3f> target_positions_aos;
    TArray<FVector3f> target_velocities_aos;
    shooter_positions_aos.SetNumUninitialized(count);
    target_positions_aos.SetNumUninitialized(count);
    target_velocities_aos.SetNumUninitialized(count);

    for (int32 i{0}; i < count; ++i) {
        auto const value{static_cast<float>(i)};
        shooter_positions_aos[i] = FVector3f{value * 0.25f, value * -0.125f, value * 0.0625f};
        target_positions_aos[i] = shooter_positions_aos[i] + FVector3f{1000.0f + value, 250.0f - value * 0.25f, 100.0f};
        target_velocities_aos[i] = FVector3f{25.0f, -10.0f + static_cast<float>(i % 20), 5.0f};
    }

    auto const shooter_positions{ml::make_vectors3f(shooter_positions_aos)};
    auto const target_positions{ml::make_vectors3f(target_positions_aos)};
    auto const target_velocities{ml::make_vectors3f(target_velocities_aos)};

    TArray<float> intercept_times;
    intercept_times.SetNumUninitialized(count);

    auto benchmark_impl{[&]<auto SolveInterceptTimes>(char const* name) {
        BENCHMARK(name) {
            return SolveInterceptTimes(
                intercept_times, shooter_positions.get_view(), target_positions.get_view(), target_velocities.get_view(), projectile_speed);
        };
    }};

    benchmark_impl.template operator()<ml::detail::solve_intercept_times_loop::solve_intercept_times>(
        "ml::detail::solve_intercept_times_loop");

    benchmark_impl.template operator()<ml::detail::solve_intercept_times_aos::solve_intercept_times>(
        "ml::detail::solve_intercept_times_aos");
}
