#include <ioj/projectile_intercept.h>
#include <ioj/vector_value_math.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace {
struct InterceptCase {
    ioj::Vector3f shooter_position;
    ioj::Vector3f target_position;
    ioj::Vector3f target_velocity;
    float projectile_speed;
    float expected_time;
};

void expect_intercept_geometry(InterceptCase const& test_case, float const intercept_time) {
    auto const target{test_case.target_position + test_case.target_velocity * intercept_time};
    auto const target_distance{ioj::size(target - test_case.shooter_position)};
    EXPECT_NEAR(test_case.projectile_speed * intercept_time, target_distance, 1e-4f);
}

using BatchSolver = void (*)(float*,
                             ioj::vectors3f::ResolvedConstView,
                             ioj::vectors3f::ResolvedConstView,
                             ioj::vectors3f::ResolvedConstView,
                             float) noexcept;
}

using ScalarIntercept = testing::TestWithParam<InterceptCase>;
using RejectedIntercept = testing::TestWithParam<InterceptCase>;

TEST_P(ScalarIntercept, SolvesScalarCases) {
    auto const& test{GetParam()};
    auto const result{ioj::solve_intercept_time(
        test.shooter_position, test.target_position, test.target_velocity, test.projectile_speed)};
    EXPECT_NEAR(result, test.expected_time, 1e-5f);
    expect_intercept_geometry(test, result);
}

constexpr InterceptCase scalar_intercept_cases[]{
    {{}, {10, 0, 0}, {}, 5, 2},
    {{}, {10, 0, 0}, {1, 0, 0}, 5, 2.5f},
    {{}, {10, 0, 0}, {-5, 0, 0}, 5, 1},
    {{}, {10, 0, 0}, {-5, 5, 0}, 5, 2},
    {{}, {10, 0, 0}, {-10, 0, 0}, 5, 2.f / 3.f},
};
INSTANTIATE_TEST_SUITE_P(NativeCoreProjectileIntercept,
                         ScalarIntercept,
                         testing::ValuesIn(scalar_intercept_cases));

TEST_P(RejectedIntercept, RejectsDegenerateAndEscapingTargets) {
    auto const& test{GetParam()};
    EXPECT_EQ(ioj::solve_intercept_time(test.shooter_position,
                                        test.target_position,
                                        test.target_velocity,
                                        test.projectile_speed),
              test.expected_time);
}

constexpr InterceptCase rejected_intercept_cases[]{
    {{}, {10, 0, 0}, {5, 0, 0}, 5, 0},
    {{}, {10, 0, 0}, {0, 10, 0}, 5, 0},
    {{3, 4, 5}, {3, 4, 5}, {1, 2, 3}, 20, 0},
};
INSTANTIATE_TEST_SUITE_P(NativeCoreProjectileIntercept,
                         RejectedIntercept,
                         testing::ValuesIn(rejected_intercept_cases));

TEST(NativeCoreProjectileIntercept, BatchImplementationsMatchExpectedResults) {
    std::array<float, 5> const shooter_x{100.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 5> const shooter_y{-50.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 5> const shooter_z{25.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 5> const target_x{110.0f, 10.0f, 10.0f, 10.0f, 10.0f};
    std::array<float, 5> const target_y{-50.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 5> const target_z{25.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 5> const velocity_x{0.0f, -5.0f, -5.0f, 5.0f, 0.0f};
    std::array<float, 5> const velocity_y{0.0f, 0.0f, 5.0f, 0.0f, 10.0f};
    std::array<float, 5> const velocity_z{};
    std::array<float, 5> const expected{2.0f, 1.0f, 2.0f, 0.0f, 0.0f};

    auto const count{static_cast<std::int32_t>(expected.size())};
    auto const shooters{ioj::vectors3f::ResolvedConstView{
        shooter_x.data(), shooter_y.data(), shooter_z.data(), count}};
    auto const targets{ioj::vectors3f::ResolvedConstView{
        target_x.data(), target_y.data(), target_z.data(), count}};
    auto const velocities{ioj::vectors3f::ResolvedConstView{
        velocity_x.data(), velocity_y.data(), velocity_z.data(), count}};
    constexpr BatchSolver solvers[]{
        ioj::detail::solve_intercept_times_aos::solve_intercept_times,
        ioj::detail::solve_intercept_times_struct_loop::solve_intercept_times,
        ioj::detail::solve_intercept_times_soa_loop::solve_intercept_times,
    };

    for (auto const solve : solvers) {
        std::array<float, 5> results{};
        solve(results.data(), shooters, targets, velocities, 5.0f);
        for (std::size_t i{}; i < results.size(); ++i) {
            EXPECT_NEAR(results[i], expected[i], 1e-5f);
        }
    }
}
