#include <SandboxCore/array_utils.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
struct FInterceptCase {
    FVector3f shooter_position;
    FVector3f target_position;
    FVector3f target_velocity;
    float projectile_speed;
    float expected_time;
};

auto make_case_vectors(TConstArrayView<FInterceptCase> const cases, auto FInterceptCase::* member) -> FVectors3f {
    TArray<FVector3f> values;
    values.Reserve(cases.Num());
    for (auto const& test_case : cases) {
        values.Add(test_case.*member);
    }
    return ml::make_vectors3f(values);
}

void check_intercept_geometry(FInterceptCase const& test_case, float const intercept_time) {
    auto const target_at_intercept{test_case.target_position + test_case.target_velocity * intercept_time};
    auto const projectile_distance{test_case.projectile_speed * intercept_time};
    auto const target_distance{FVector3f::Distance(test_case.shooter_position, target_at_intercept)};
    CHECK(FMath::IsNearlyEqual(projectile_distance, target_distance, 1e-4f));
}
}

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

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_time.CoversLinearAndTangentSolutions") {
    TArray<FInterceptCase> const cases{
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {-5.f, 0.f, 0.f}, 5.f, 1.f},
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {-5.f, 5.f, 0.f}, 5.f, 2.f},
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {-10.f, 0.f, 0.f}, 5.f, 2.f / 3.f},
    };

    for (auto const& test_case : cases) {
        auto const result{ml::solve_intercept_time(
            test_case.shooter_position, test_case.target_position, test_case.target_velocity, test_case.projectile_speed)};
        CHECK(FMath::IsNearlyEqual(result, test_case.expected_time, 1e-5f));
        check_intercept_geometry(test_case, result);
    }
}

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_time.RejectsDegenerateAndEscapingTargets") {
    TArray<FInterceptCase> const cases{
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {5.f, 0.f, 0.f}, 5.f, 0.f},
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {0.f, 10.f, 0.f}, 5.f, 0.f},
        {{3.f, 4.f, 5.f}, {3.f, 4.f, 5.f}, {1.f, 2.f, 3.f}, 20.f, 0.f},
    };

    for (auto const& test_case : cases) {
        CHECK(ml::solve_intercept_time(
                  test_case.shooter_position, test_case.target_position, test_case.target_velocity, test_case.projectile_speed) ==
              test_case.expected_time);
    }
}

TEST_CASE("SandboxCore.ProjectileIntercept.solve_intercept_times.MatchesIndependentExpectedResults") {
    TArray<FInterceptCase> const cases{
        {{100.f, -50.f, 25.f}, {110.f, -50.f, 25.f}, {0.f, 0.f, 0.f}, 5.f, 2.f},
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {-5.f, 0.f, 0.f}, 5.f, 1.f},
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {-5.f, 5.f, 0.f}, 5.f, 2.f},
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {5.f, 0.f, 0.f}, 5.f, 0.f},
        {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {0.f, 10.f, 0.f}, 5.f, 0.f},
    };
    auto const shooter_positions{make_case_vectors(cases, &FInterceptCase::shooter_position)};
    auto const target_positions{make_case_vectors(cases, &FInterceptCase::target_position)};
    auto const target_velocities{make_case_vectors(cases, &FInterceptCase::target_velocity)};
    TArray<float> results;
    results.SetNumUninitialized(cases.Num());

    auto const check_implementation{[&]<auto SolveInterceptTimes>(char const* const name) {
        SECTION(name) {
            SolveInterceptTimes(
                results, shooter_positions.get_const_view(), target_positions.get_const_view(), target_velocities.get_const_view(), 5.f);

            for (int32 i{}; i < cases.Num(); ++i) {
                CHECK(FMath::IsNearlyEqual(results[i], cases[i].expected_time, 1e-5f));
                if (results[i] > 0.f) {
                    check_intercept_geometry(cases[i], results[i]);
                }
            }
        }
    }};

    check_implementation.template operator()<ml::solve_intercept_times>("soa loop");
    check_implementation.template operator()<ml::detail::solve_intercept_times_struct_loop::solve_intercept_times>("struct loop");
    check_implementation.template operator()<ml::detail::solve_intercept_times_aos::solve_intercept_times>("aos loop");
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

    test_impl.template operator()<ml::detail::solve_intercept_times_struct_loop::solve_intercept_times>(
        "ml::detail::solve_intercept_times_struct_loop");

    test_impl.template operator()<ml::detail::solve_intercept_times_aos::solve_intercept_times>("ml::detail::solve_intercept_times_aos");
}
