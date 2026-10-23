#include <ioj/projectile_intercept.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace {
struct InterceptCase {
    ioj::Vector3f shooter_position;
    ioj::Vector3f target_position;
    ioj::Vector3f target_velocity;
    float projectile_speed;
    float expected_time;
};

constexpr auto v3(float const x, float const y, float const z) noexcept -> ioj::Vector3f {
    return ioj::Vector3f{x, y, z};
}

void expect_intercept_geometry(InterceptCase const& test_case, float const intercept_time) {
    auto const target_x{test_case.target_position.x() +
                        test_case.target_velocity.x() * intercept_time};
    auto const target_y{test_case.target_position.y() +
                        test_case.target_velocity.y() * intercept_time};
    auto const target_z{test_case.target_position.z() +
                        test_case.target_velocity.z() * intercept_time};
    auto const dx{target_x - test_case.shooter_position.x()};
    auto const dy{target_y - test_case.shooter_position.y()};
    auto const dz{target_z - test_case.shooter_position.z()};
    auto const target_distance{std::sqrt(dx * dx + dy * dy + dz * dz)};
    EXPECT_NEAR(test_case.projectile_speed * intercept_time, target_distance, 1e-4f);
}

using BatchSolver = void (*)(float*,
                             ioj::vectors3f::ResolvedConstView,
                             ioj::vectors3f::ResolvedConstView,
                             ioj::vectors3f::ResolvedConstView,
                             float) noexcept;
}

TEST(NativeCoreProjectileIntercept, SolvesScalarCases) {
    constexpr InterceptCase cases[]{
        {v3(0.0f, 0.0f, 0.0f), v3(10.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, 0.0f), 5.0f, 2.0f},
        {v3(0.0f, 0.0f, 0.0f), v3(10.0f, 0.0f, 0.0f), v3(1.0f, 0.0f, 0.0f), 5.0f, 2.5f},
        {v3(0.0f, 0.0f, 0.0f), v3(10.0f, 0.0f, 0.0f), v3(-5.0f, 0.0f, 0.0f), 5.0f, 1.0f},
        {v3(0.0f, 0.0f, 0.0f), v3(10.0f, 0.0f, 0.0f), v3(-5.0f, 5.0f, 0.0f), 5.0f, 2.0f},
        {v3(0.0f, 0.0f, 0.0f), v3(10.0f, 0.0f, 0.0f), v3(-10.0f, 0.0f, 0.0f), 5.0f, 2.0f / 3.0f},
    };

    for (auto const& test_case : cases) {
        auto const result{ioj::solve_intercept_time(test_case.shooter_position,
                                                    test_case.target_position,
                                                    test_case.target_velocity,
                                                    test_case.projectile_speed)};
        EXPECT_NEAR(result, test_case.expected_time, 1e-5f);
        expect_intercept_geometry(test_case, result);
    }
}

TEST(NativeCoreProjectileIntercept, RejectsDegenerateAndEscapingTargets) {
    constexpr InterceptCase cases[]{
        {v3(0.0f, 0.0f, 0.0f), v3(10.0f, 0.0f, 0.0f), v3(5.0f, 0.0f, 0.0f), 5.0f, 0.0f},
        {v3(0.0f, 0.0f, 0.0f), v3(10.0f, 0.0f, 0.0f), v3(0.0f, 10.0f, 0.0f), 5.0f, 0.0f},
        {v3(3.0f, 4.0f, 5.0f), v3(3.0f, 4.0f, 5.0f), v3(1.0f, 2.0f, 3.0f), 20.0f, 0.0f},
    };

    for (auto const& test_case : cases) {
        EXPECT_EQ(ioj::solve_intercept_time(test_case.shooter_position,
                                            test_case.target_position,
                                            test_case.target_velocity,
                                            test_case.projectile_speed),
                  test_case.expected_time);
    }
}

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
    auto const targets{
        ioj::vectors3f::ResolvedConstView{target_x.data(), target_y.data(), target_z.data(), count}};
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
