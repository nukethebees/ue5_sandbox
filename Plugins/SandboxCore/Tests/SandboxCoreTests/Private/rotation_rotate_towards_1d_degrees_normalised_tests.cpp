#include "SandboxCore/rotation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
struct FRotateTowardsCase {
    char const* name;
    float current;
    float target;
    float speed;
    float delta_time;
    float expected;
};

void check_single_rotation(FRotateTowardsCase const& test_case) {
    float const current_values[]{test_case.current};
    float const target_values[]{test_case.target};
    float out[]{999.0f};

    ml::kernel::rotate_towards_1d_degrees_normalised(current_values, target_values, test_case.speed, test_case.delta_time, out, 1);

    CHECK(out[0] == test_case.expected);
}

void check_rotate_towards_in_place(
    TArray<float> current, TArray<float> const& target, float const speed, float const delta_time, TArray<float> const& expected) {
    REQUIRE(current.Num() == target.Num());
    REQUIRE(current.Num() == expected.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised_in_place(current.GetData(), target.GetData(), speed, delta_time, current.Num());

    CHECK(current == expected);
}

void check_rotate_towards_in_place(FRotateTowardsCase const& test_case) {
    auto current{TArray<float>{test_case.current}};
    auto const target{TArray<float>{test_case.target}};

    ml::kernel::rotate_towards_1d_degrees_normalised_in_place(
        current.GetData(), target.GetData(), test_case.speed, test_case.delta_time, current.Num());

    CHECK(current[0] == test_case.expected);
}
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised.Named scalar cases", "[SandboxCore][Math][RotateTowards1DDegreesNormalised]") {
    constexpr FRotateTowardsCase cases[]{
        {"already at zero", 0.0f, 0.0f, 90.0f, 1.0f, 0.0f},
        {"already at positive target", 90.0f, 90.0f, 90.0f, 1.0f, 90.0f},
        {"already at negative target", -90.0f, -90.0f, 90.0f, 1.0f, -90.0f},
        {"already at negative half-turn", -180.0f, -180.0f, 90.0f, 1.0f, -180.0f},
        {"clockwise from zero", 0.0f, 90.0f, 30.0f, 1.0f, 30.0f},
        {"clockwise uses speed times delta time", 10.0f, 100.0f, 20.0f, 2.0f, 50.0f},
        {"clockwise from negative angle", -90.0f, 0.0f, 45.0f, 1.0f, -45.0f},
        {"counter-clockwise from positive angle", 90.0f, 0.0f, 30.0f, 1.0f, 60.0f},
        {"counter-clockwise uses speed times delta time", 100.0f, 10.0f, 20.0f, 2.0f, 60.0f},
        {"counter-clockwise from zero", 0.0f, -90.0f, 45.0f, 1.0f, -45.0f},
        {"clockwise overshoot snaps", 0.0f, 10.0f, 90.0f, 1.0f, 10.0f},
        {"counter-clockwise overshoot snaps", 0.0f, -10.0f, 90.0f, 1.0f, -10.0f},
        {"nearby clockwise target snaps", 45.0f, 50.0f, 10.0f, 1.0f, 50.0f},
        {"nearby counter-clockwise target snaps", 45.0f, 40.0f, 10.0f, 1.0f, 40.0f},
        {"moves towards positive boundary", 170.0f, -170.0f, 5.0f, 1.0f, 175.0f},
        {"wraps across positive boundary", 175.0f, -170.0f, 10.0f, 1.0f, -175.0f},
        {"positive boundary reaches negative half-turn", 179.0f, -179.0f, 1.0f, 1.0f, -180.0f},
        {"moves towards negative boundary", -170.0f, 170.0f, 5.0f, 1.0f, -175.0f},
        {"wraps across negative boundary", -175.0f, 170.0f, 10.0f, 1.0f, 175.0f},
        {"negative boundary reaches negative half-turn", -179.0f, 179.0f, 1.0f, 1.0f, -180.0f},
        {"snaps across positive boundary", 170.0f, -170.0f, 90.0f, 1.0f, -170.0f},
        {"snaps across negative boundary", -170.0f, 170.0f, 90.0f, 1.0f, 170.0f},
        {"shortest path from 179 to -179", 179.0f, -179.0f, 90.0f, 1.0f, -179.0f},
        {"shortest path from -179 to 179", -179.0f, 179.0f, 90.0f, 1.0f, 179.0f},
        {"zero speed from zero", 0.0f, 90.0f, 0.0f, 1.0f, 0.0f},
        {"zero speed from positive angle", 90.0f, 0.0f, 0.0f, 1.0f, 90.0f},
        {"zero speed across boundary", 170.0f, -170.0f, 0.0f, 1.0f, 170.0f},
        {"zero delta time from zero", 0.0f, 90.0f, 90.0f, 0.0f, 0.0f},
        {"zero delta time from positive angle", 90.0f, 0.0f, 90.0f, 0.0f, 90.0f},
        {"zero delta time across boundary", -170.0f, 170.0f, 90.0f, 0.0f, -170.0f},
    };

    for (auto const& test_case : cases) {
        DYNAMIC_SECTION(test_case.name) {
            check_single_rotation(test_case);
        }
    }
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised.does nothing when count is zero",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised]") {
    float const current[]{0.0f};
    float const target[]{90.0f};
    float out[]{123.0f};

    ml::kernel::rotate_towards_1d_degrees_normalised(current, target, 90.0f, 1.0f, out, 0);

    CHECK(out[0] == 123.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised.processes multiple clockwise elements",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][MultipleElements]") {
    TArray<float> const current{0.0f, 10.0f, -90.0f};
    TArray<float> const target{90.0f, 100.0f, 0.0f};
    TArray<float> out;
    out.Init(999.0f, current.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised(current.GetData(), target.GetData(), 30.0f, 1.0f, out.GetData(), out.Num());

    CHECK((out == TArray<float>{30.0f, 40.0f, -60.0f}));
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised.processes multiple anticlockwise elements",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][MultipleElements]") {
    TArray<float> const current{90.0f, 100.0f, 0.0f};
    TArray<float> const target{0.0f, 10.0f, -90.0f};
    TArray<float> out;
    out.Init(999.0f, current.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised(current.GetData(), target.GetData(), 30.0f, 1.0f, out.GetData(), out.Num());

    CHECK((out == TArray<float>{60.0f, 70.0f, -30.0f}));
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised.processes multiple snapping elements",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][MultipleElements]") {
    TArray<float> const current{10.0f, -10.0f, 45.0f, 45.0f};
    TArray<float> const target{20.0f, -20.0f, 50.0f, 40.0f};
    TArray<float> out;
    out.Init(999.0f, current.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised(current.GetData(), target.GetData(), 30.0f, 1.0f, out.GetData(), out.Num());

    CHECK((out == TArray<float>{20.0f, -20.0f, 50.0f, 40.0f}));
}

TEST_CASE("SandboxCore.Math.RotateTowards1DDegreesNormalised.processes multiple wrap-boundary elements",
          "[SandboxCore][Math][RotateTowards1DDegreesNormalised][MultipleElements]") {
    TArray<float> const current{170.0f, -170.0f, 175.0f, -175.0f};
    TArray<float> const target{-170.0f, 170.0f, -170.0f, 170.0f};
    TArray<float> out;
    out.Init(999.0f, current.Num());

    ml::kernel::rotate_towards_1d_degrees_normalised(current.GetData(), target.GetData(), 10.0f, 1.0f, out.GetData(), out.Num());

    CHECK((out == TArray<float>{-180.0f, -180.0f, -175.0f, 175.0f}));
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace.Named scalar cases", "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    constexpr FRotateTowardsCase cases[]{
        {"rotates clockwise", 0.0f, 90.0f, 30.0f, 1.0f, 30.0f},
        {"rotates counter-clockwise", 90.0f, 0.0f, 30.0f, 1.0f, 60.0f},
        {"snaps clockwise", 0.0f, 30.0f, 90.0f, 1.0f, 30.0f},
        {"snaps counter-clockwise", 30.0f, 0.0f, 90.0f, 1.0f, 0.0f},
        {"uses delta time", 0.0f, 90.0f, 60.0f, 0.5f, 30.0f},
        {"crosses positive wrap boundary", 170.0f, -170.0f, 10.0f, 1.0f, -180.0f},
        {"crosses negative wrap boundary", -170.0f, 170.0f, 10.0f, 1.0f, -180.0f},
    };

    for (auto const& test_case : cases) {
        DYNAMIC_SECTION(test_case.name) {
            check_rotate_towards_in_place(test_case);
        }
    }
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace.does nothing when already at target",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_in_place({0.0f, 90.0f, -90.0f}, {0.0f, 90.0f, -90.0f}, 90.0f, 1.0f, {0.0f, 90.0f, -90.0f});
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace.does nothing when speed is zero",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_in_place({0.0f, 45.0f, -45.0f}, {90.0f, 90.0f, -90.0f}, 0.0f, 1.0f, {0.0f, 45.0f, -45.0f});
}

TEST_CASE("SandboxCore.Math.RotateTowards1DNormalisedInplace.handles mixed batch",
          "[SandboxCore][Math][RotateTowards1DNormalisedInplace]") {
    check_rotate_towards_in_place(
        {0.0f, 90.0f, 170.0f, -170.0f}, {90.0f, 0.0f, -170.0f, 170.0f}, 10.0f, 1.0f, {10.0f, 80.0f, -180.0f, -180.0f});
}
