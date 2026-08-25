#include "SandboxCore/rotation.h"

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <concepts>

namespace {
template <std::floating_point T>
struct TDesiredYawCase {
    char const* name;
    T start_x;
    T start_y;
    T end_x;
    T end_y;
    T expected;
};

template <std::floating_point T>
void check_desired_yaw_cases() {
    TDesiredYawCase<T> const cases[]{
        {"positive X", T{0}, T{0}, T{1}, T{0}, T{0}},
        {"positive Y", T{0}, T{0}, T{0}, T{1}, static_cast<T>(UE_HALF_PI)},
        {"negative X", T{0}, T{0}, T{-1}, T{0}, static_cast<T>(UE_PI)},
        {"negative Y", T{0}, T{0}, T{0}, T{-1}, static_cast<T>(-UE_HALF_PI)},
        {"upper-right diagonal", T{10}, T{20}, T{11}, T{21}, static_cast<T>(UE_PI / 4)},
        {"upper-left diagonal", T{10}, T{20}, T{9}, T{21}, static_cast<T>(3 * UE_PI / 4)},
        {"lower-left diagonal", T{10}, T{20}, T{9}, T{19}, static_cast<T>(-3 * UE_PI / 4)},
        {"lower-right diagonal", T{10}, T{20}, T{11}, T{19}, static_cast<T>(-UE_PI / 4)},
    };

    for (auto const& test_case : cases) {
        DYNAMIC_SECTION(test_case.name) {
            T const start_xs[]{test_case.start_x};
            T const start_ys[]{test_case.start_y};
            T const end_xs[]{test_case.end_x};
            T const end_ys[]{test_case.end_y};
            T out_yaws_radians[]{T{123}};

            ml::kernel::compute_desired_yaws_radians(start_xs, start_ys, end_xs, end_ys, out_yaws_radians, 1);

            constexpr T tolerance{static_cast<T>(1e-6)};
            CHECK_THAT(out_yaws_radians[0], Catch::Matchers::WithinAbs(test_case.expected, tolerance));
        }
    }
}
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians.float named cases", "[SandboxCore][Math][movement]") {
    check_desired_yaw_cases<float>();
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians.double named cases", "[SandboxCore][Math][movement]") {
    check_desired_yaw_cases<double>();
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians.Handles zero elements", "[SandboxCore][Math][movement]") {
    float out_yaws_radians[]{123.0f};

    ml::kernel::compute_desired_yaws_radians<float>(nullptr, nullptr, nullptr, nullptr, out_yaws_radians, 0);

    CHECK(out_yaws_radians[0] == 123.0f);
}
