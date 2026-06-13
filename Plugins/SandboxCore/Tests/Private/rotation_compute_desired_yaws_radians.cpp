#include "SandboxCore/rotation.h"

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {

template <std::floating_point T>
void check_desired_yaw_radians(T const start_x, T const start_y, T const end_x, T const end_y, T const expected_yaw_radians) {
    T const start_xs[]{start_x};
    T const start_ys[]{start_y};
    T const end_xs[]{end_x};
    T const end_ys[]{end_y};
    T out_yaws_radians[]{T{123}};

    ml::kernel::compute_desired_yaws_radians(start_xs, start_ys, end_xs, end_ys, out_yaws_radians, 1);

    constexpr T abs{static_cast<T>(1e-6)};

    REQUIRE_THAT(out_yaws_radians[0], Catch::Matchers::WithinAbs(expected_yaw_radians, abs));
}
} // namespace

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<float>.Computes zero yaw for positive X", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<float>(0, 0, 1, 0, 0);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<double>.Computes zero yaw for positive X", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<double>(0, 0, 1, 0, 0);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<float>.Computes positive half pi for positive Y", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<float>(0, 0, 0, 1, UE_HALF_PI);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<double>.Computes positive half pi for positive Y", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<double>(0, 0, 0, 1, UE_HALF_PI);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<float>.Computes pi for negative X", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<float>(0, 0, -1, 0, UE_PI);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<double>.Computes pi for negative X", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<double>(0, 0, -1, 0, UE_PI);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<float>.Computes negative half pi for negative Y", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<float>(0, 0, 0, -1, -UE_HALF_PI);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<double>.Computes negative half pi for negative Y", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<double>(0, 0, 0, -1, -UE_HALF_PI);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<float>.Computes upper right diagonal yaw", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<float>(10, 20, 11, 21, UE_PI / 4);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<double>.Computes upper right diagonal yaw", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<double>(10, 20, 11, 21, UE_PI / 4);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<float>.Computes upper left diagonal yaw", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<float>(10, 20, 9, 21, 3 * UE_PI / 4);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<double>.Computes upper left diagonal yaw", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<double>(10, 20, 9, 21, 3 * UE_PI / 4);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<float>.Computes lower left diagonal yaw", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<float>(10, 20, 9, 19, -3 * UE_PI / 4);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<double>.Computes lower left diagonal yaw", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<double>(10, 20, 9, 19, -3 * UE_PI / 4);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<float>.Computes lower right diagonal yaw", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<float>(10, 20, 11, 19, -UE_PI / 4);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians<double>.Computes lower right diagonal yaw", "[SandboxCore][Math][movement]") {
    check_desired_yaw_radians<double>(10, 20, 11, 19, -UE_PI / 4);
}

TEST_CASE("SandboxCore.Math.ComputeDesiredYawsRadians.Handles zero elements", "[SandboxCore][Math][movement]") {
    float out_yaws_radians[]{123.0f};

    ml::kernel::compute_desired_yaws_radians<float>(nullptr, nullptr, nullptr, nullptr, out_yaws_radians, 0);

    CHECK(out_yaws_radians[0] == 123.0f);
}
