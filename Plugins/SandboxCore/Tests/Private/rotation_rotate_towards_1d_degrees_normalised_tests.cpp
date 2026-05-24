#include "SandboxCore/Public/rotation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
void check_single_rotation(float const current, float const target, float const speed, float const delta_time, float const expected) {
    float const current_values[]{current};
    float const target_values[]{target};
    float out[]{999.0f};

    ml::kernel::rotate_towards_1d_degrees_normalised(current_values, target_values, speed, delta_time, out, 1);

    CHECK(out[0] == expected);
}
} // namespace

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised does nothing when count is zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised]") {
    float const current[]{0.0f};
    float const target[]{90.0f};
    float out[]{123.0f};

    ml::kernel::rotate_towards_1d_degrees_normalised(current, target, 90.0f, 1.0f, out, 0);

    CHECK(out[0] == 123.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised returns zero when already at zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][AlreadyAtTarget]") {
    check_single_rotation(0.0f, 0.0f, 90.0f, 1.0f, 0.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised returns positive angle when already at positive target",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][AlreadyAtTarget]") {
    check_single_rotation(90.0f, 90.0f, 90.0f, 1.0f, 90.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised returns negative angle when already at negative target",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][AlreadyAtTarget]") {
    check_single_rotation(-90.0f, -90.0f, 90.0f, 1.0f, -90.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised returns negative half turn when already there",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][AlreadyAtTarget]") {
    check_single_rotation(-180.0f, -180.0f, 90.0f, 1.0f, -180.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised moves clockwise from zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Clockwise]") {
    check_single_rotation(0.0f, 90.0f, 30.0f, 1.0f, 30.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised moves clockwise using speed times delta time",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Clockwise]") {
    check_single_rotation(10.0f, 100.0f, 20.0f, 2.0f, 50.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised moves clockwise from negative angle",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Clockwise]") {
    check_single_rotation(-90.0f, 0.0f, 45.0f, 1.0f, -45.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised moves anticlockwise from positive angle",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Anticlockwise]") {
    check_single_rotation(90.0f, 0.0f, 30.0f, 1.0f, 60.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised moves anticlockwise using speed times delta time",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Anticlockwise]") {
    check_single_rotation(100.0f, 10.0f, 20.0f, 2.0f, 60.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised moves anticlockwise from zero to negative angle",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Anticlockwise]") {
    check_single_rotation(0.0f, -90.0f, 45.0f, 1.0f, -45.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised snaps clockwise to target when step would overshoot",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Snap]") {
    check_single_rotation(0.0f, 10.0f, 90.0f, 1.0f, 10.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised snaps anticlockwise to target when step would overshoot",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Snap]") {
    check_single_rotation(0.0f, -10.0f, 90.0f, 1.0f, -10.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised snaps to nearby clockwise target",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Snap]") {
    check_single_rotation(45.0f, 50.0f, 10.0f, 1.0f, 50.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised snaps to nearby anticlockwise target",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][Snap]") {
    check_single_rotation(45.0f, 40.0f, 10.0f, 1.0f, 40.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised moves towards positive boundary before wrapping",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][PositiveBoundary]") {
    check_single_rotation(170.0f, -170.0f, 5.0f, 1.0f, 175.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised wraps from positive boundary to negative side",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][PositiveBoundary]") {
    check_single_rotation(175.0f, -170.0f, 10.0f, 1.0f, -175.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised stays at negative half turn",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][PositiveBoundary]") {
    check_single_rotation(179.0f, -179.0f, 1.0f, 1.0f, -180.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised moves towards negative boundary before wrapping",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][NegativeBoundary]") {
    check_single_rotation(-170.0f, 170.0f, 5.0f, 1.0f, -175.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised wraps from negative boundary to positive side",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][NegativeBoundary]") {
    check_single_rotation(-175.0f, 170.0f, 10.0f, 1.0f, 175.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised reaches negative half turn at boundary",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][NegativeBoundary]") {
    check_single_rotation(-179.0f, 179.0f, 1.0f, 1.0f, -180.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised snaps across wrap boundary from positive to negative",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ShortestPath]") {
    check_single_rotation(170.0f, -170.0f, 90.0f, 1.0f, -170.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised snaps across wrap boundary from negative to positive",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ShortestPath]") {
    check_single_rotation(-170.0f, 170.0f, 90.0f, 1.0f, 170.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised takes shortest path from 179 to -179",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ShortestPath]") {
    check_single_rotation(179.0f, -179.0f, 90.0f, 1.0f, -179.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised takes shortest path from -179 to 179",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ShortestPath]") {
    check_single_rotation(-179.0f, 179.0f, 90.0f, 1.0f, 179.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised does not move from zero when speed is zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ZeroSpeed]") {
    check_single_rotation(0.0f, 90.0f, 0.0f, 1.0f, 0.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised does not move from positive angle when speed is zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ZeroSpeed]") {
    check_single_rotation(90.0f, 0.0f, 0.0f, 1.0f, 90.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised does not move across boundary when speed is zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ZeroSpeed]") {
    check_single_rotation(170.0f, -170.0f, 0.0f, 1.0f, 170.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised does not move from zero when delta time is zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ZeroDeltaTime]") {
    check_single_rotation(0.0f, 90.0f, 90.0f, 0.0f, 0.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised does not move from positive angle when delta time is zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ZeroDeltaTime]") {
    check_single_rotation(90.0f, 0.0f, 90.0f, 0.0f, 90.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised does not move across boundary when delta time is zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][ZeroDeltaTime]") {
    check_single_rotation(-170.0f, 170.0f, 90.0f, 0.0f, -170.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised processes multiple clockwise elements",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][MultipleElements]") {
    TArray<float> const current{0.0f, 10.0f, -90.0f};
    TArray<float> const target{90.0f, 100.0f, 0.0f};

    TArray<float> out;
    out.Init(999.0f, current.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised(current.GetData(), target.GetData(), 30.0f, 1.0f, out.GetData(), out.Num());

    CHECK(out[0] == 30.0f);
    CHECK(out[1] == 40.0f);
    CHECK(out[2] == -60.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised processes multiple anticlockwise elements",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][MultipleElements]") {
    TArray<float> const current{90.0f, 100.0f, 0.0f};
    TArray<float> const target{0.0f, 10.0f, -90.0f};

    TArray<float> out;
    out.Init(999.0f, current.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised(current.GetData(), target.GetData(), 30.0f, 1.0f, out.GetData(), out.Num());

    CHECK(out[0] == 60.0f);
    CHECK(out[1] == 70.0f);
    CHECK(out[2] == -30.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised processes multiple snapping elements",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][MultipleElements]") {
    TArray<float> const current{10.0f, -10.0f, 45.0f, 45.0f};
    TArray<float> const target{20.0f, -20.0f, 50.0f, 40.0f};

    TArray<float> out;
    out.Init(999.0f, current.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised(current.GetData(), target.GetData(), 30.0f, 1.0f, out.GetData(), out.Num());

    CHECK(out[0] == 20.0f);
    CHECK(out[1] == -20.0f);
    CHECK(out[2] == 50.0f);
    CHECK(out[3] == 40.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised processes multiple wrap-boundary elements",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][MultipleElements]") {
    TArray<float> const current{170.0f, -170.0f, 175.0f, -175.0f};
    TArray<float> const target{-170.0f, 170.0f, -170.0f, 170.0f};

    TArray<float> out;
    out.Init(999.0f, current.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised(current.GetData(), target.GetData(), 10.0f, 1.0f, out.GetData(), out.Num());

    CHECK(out[0] == -180.0f);
    CHECK(out[1] == -180.0f);
    CHECK(out[2] == -175.0f);
    CHECK(out[3] == 175.0f);
}

// -------------------------------------------------------------------------------------------------
// check_rotate_towards_inplace
// -------------------------------------------------------------------------------------------------

namespace {
void check_rotate_towards_inplace(
    TArray<float> current, TArray<float> const& target, float const speed, float const delta_time, TArray<float> const& expected) {
    REQUIRE(current.Num() == target.Num());
    REQUIRE(current.Num() == expected.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised_inplace(current.GetData(), target.GetData(), speed, delta_time, current.Num());

    CHECK(current == expected);
}

void
    check_rotate_towards_inplace(float const current, float const target, float const speed, float const delta_time, float const expected) {
    auto current_array{TArray<float>{current}};
    auto const target_array{TArray<float>{target}};

    ml::kernel::rotate_towards_1d_degrees_normalised_inplace(
        current_array.GetData(), target_array.GetData(), speed, delta_time, current_array.Num());

    CHECK(current_array[0] == expected);
}
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace does nothing when already at target",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_inplace({0.0f, 90.0f, -90.0f}, {0.0f, 90.0f, -90.0f}, 90.0f, 1.0f, {0.0f, 90.0f, -90.0f});
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace rotates towards target without overshoot",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_inplace(0.0f, 90.0f, 30.0f, 1.0f, 30.0f);
    check_rotate_towards_inplace(90.0f, 0.0f, 30.0f, 1.0f, 60.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace snaps to target when step reaches target",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_inplace(0.0f, 30.0f, 90.0f, 1.0f, 30.0f);
    check_rotate_towards_inplace(30.0f, 0.0f, 90.0f, 1.0f, 0.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace uses delta time", "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_inplace(0.0f, 90.0f, 60.0f, 0.5f, 30.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace does nothing when speed is zero",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_inplace({0.0f, 45.0f, -45.0f}, {90.0f, 90.0f, -90.0f}, 0.0f, 1.0f, {0.0f, 45.0f, -45.0f});
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace rotates across positive wrap boundary",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_inplace(170.0f, -170.0f, 10.0f, 1.0f, -180.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace rotates across negative wrap boundary",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_inplace(-170.0f, 170.0f, 10.0f, 1.0f, -180.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace handles mixed batch",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_inplace(
        {0.0f, 90.0f, 170.0f, -170.0f}, {90.0f, 0.0f, -170.0f, 170.0f}, 10.0f, 1.0f, {10.0f, 80.0f, -180.0f, -180.0f});
}
