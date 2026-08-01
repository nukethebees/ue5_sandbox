#include <SandboxCore/array_utils.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_time.StationaryTarget") {
    auto const result{
        ml::solve_intercept_time(FVector3f{0.0f, 0.0f, 0.0f}, FVector3f{10.0f, 0.0f, 0.0f}, FVector3f{0.0f, 0.0f, 0.0f}, 5.0f)};

    CHECK(FMath::IsNearlyEqual(result, 2.0f));
}

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_time.MovingTarget") {
    auto const result{
        ml::solve_intercept_time(FVector3f{0.0f, 0.0f, 0.0f}, FVector3f{10.0f, 0.0f, 0.0f}, FVector3f{1.0f, 0.0f, 0.0f}, 5.0f)};

    CHECK(FMath::IsNearlyEqual(result, 2.5f));
}

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_time.NoIntercept") {
    auto const result{
        ml::solve_intercept_time(FVector3f{0.0f, 0.0f, 0.0f}, FVector3f{10.0f, 0.0f, 0.0f}, FVector3f{10.0f, 0.0f, 0.0f}, 5.0f)};

    CHECK(FMath::IsNearlyZero(result));
}

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_times.MatchesScalar") {
    auto const shooter_positions{ml::make_vectors3f(TArray<FVector3f>{
        {0.0f, 0.0f, 0.0f},
        {5.0f, -2.0f, 1.0f},
        {-3.0f, 4.0f, 0.0f},
    })};
    auto const target_positions{ml::make_vectors3f(TArray<FVector3f>{
        {10.0f, 0.0f, 0.0f},
        {15.0f, 3.0f, 1.0f},
        {2.0f, 4.0f, 0.0f},
    })};
    auto const target_velocities{ml::make_vectors3f(TArray<FVector3f>{
        {0.0f, 0.0f, 0.0f},
        {1.0f, -0.5f, 0.0f},
        {10.0f, 0.0f, 0.0f},
    })};
    float constexpr projectile_speed{5.0f};
    TArray<float> expected;
    expected.SetNumUninitialized(shooter_positions.num());

    for (int32 i{0}; i < expected.Num(); ++i) {
        expected[i] = ml::solve_intercept_time(FVector3f{shooter_positions.xs[i], shooter_positions.ys[i], shooter_positions.zs[i]},
                                               FVector3f{target_positions.xs[i], target_positions.ys[i], target_positions.zs[i]},
                                               FVector3f{target_velocities.xs[i], target_velocities.ys[i], target_velocities.zs[i]},
                                               projectile_speed);
    }

    TArray<float> results;
    results.SetNumUninitialized(expected.Num());

    auto test_impl{[&]<auto SolveInterceptTimes>(char const* name) {
        SECTION(name) {
            SolveInterceptTimes(
                results, shooter_positions.get_view(), target_positions.get_view(), target_velocities.get_view(), projectile_speed);

            CHECK(ml::almost_equal(TConstArrayView<float>{results}, TConstArrayView<float>{expected}));
        }
    }};

    test_impl.template operator()<ml::solve_intercept_times>("ml");

    test_impl.template operator()<ml::solve_intercept_times_loop::solve_intercept_times>("ml::solve_intercept_times_loop");

    test_impl.template operator()<ml::solve_intercept_times_aos::solve_intercept_times>("ml::solve_intercept_times_aos");
}
