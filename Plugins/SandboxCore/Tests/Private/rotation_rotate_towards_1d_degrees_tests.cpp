#include "SandboxCore/Public/rotation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
constexpr float default_delta_time{1.0f};
constexpr float unwritten_value{-1.0f};

struct rotate_towards_case {
    float current;
    float target;
    float expected;
};

void check_rotate_towards(float const current, float const target, float const speed, float const delta_time, float const expected) {
    float const Current[]{current};
    float const Target[]{target};
    float Out[]{unwritten_value};

    ml::kernel::rotate_towards_1d_degrees(Current, Target, speed, delta_time, Out, 1);

    CHECK(FMath::IsNearlyEqual(Out[0], expected));
}

template <int32 Count>
void check_rotate_towards_many(float const (&current)[Count],
                               float const (&target)[Count],
                               float const speed,
                               float const delta_time,
                               float const (&expected)[Count]) {
    float Out[Count];

    for (int32 i{0}; i < Count; ++i) {
        Out[i] = unwritten_value;
    }

    ml::kernel::rotate_towards_1d_degrees(current, target, speed, delta_time, Out, Count);

    for (int32 i{0}; i < Count; ++i) {
        CHECK(Out[i] == expected[i]);
    }
}

template <int32 Count>
void check_rotate_towards_cases(rotate_towards_case const (&cases)[Count], float const speed, float const delta_time = default_delta_time) {
    float current[Count];
    float target[Count];
    float expected[Count];

    for (int32 i{0}; i < Count; ++i) {
        current[i] = cases[i].current;
        target[i] = cases[i].target;
        expected[i] = cases[i].expected;
    }

    check_rotate_towards_many(current, target, speed, delta_time, expected);
}
} // namespace

TEST_CASE("SandboxCore.Math.RotateTowards1D returns current angle when target is equal", "[SandboxCore][Math][RotateTowards1D]") {
    rotate_towards_case const Cases[]{
        {0.0f, 0.0f, 0.0f},
        {90.0f, 90.0f, 90.0f},
        {360.0f, 0.0f, 360.0f},
    };

    check_rotate_towards_cases(Cases, 90.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D rotates positively when target is clockwise", "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(0.0f, 90.0f, 30.0f, default_delta_time, 30.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D rotates negatively when target is anticlockwise", "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(90.0f, 0.0f, 30.0f, default_delta_time, 60.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D clamps to target when positive step would overshoot", "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(0.0f, 20.0f, 90.0f, default_delta_time, 20.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D clamps to target when negative step would overshoot", "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(20.0f, 0.0f, 90.0f, default_delta_time, 0.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D uses shortest path across zero when rotating positively",
          "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(350.0f, 10.0f, 5.0f, default_delta_time, 355.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D uses shortest path across zero when rotating negatively",
          "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(10.0f, 350.0f, 5.0f, default_delta_time, 5.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D clamps to wrapped target when crossing zero positively",
          "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(350.0f, 10.0f, 90.0f, default_delta_time, 10.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D clamps to wrapped target when crossing zero negatively",
          "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(10.0f, 350.0f, 90.0f, default_delta_time, 350.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D scales step by delta time", "[SandboxCore][Math][RotateTowards1D]") {
    check_rotate_towards(0.0f, 90.0f, 60.0f, 0.5f, 30.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D writes multiple rotated angles", "[SandboxCore][Math][RotateTowards1D]") {
    rotate_towards_case const Cases[]{
        {0.0f, 90.0f, 30.0f},
        {90.0f, 0.0f, 60.0f},
        {350.0f, 10.0f, 10.0f},
        {10.0f, 350.0f, 350.0f},
    };

    check_rotate_towards_cases(Cases, 30.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D does not write output when count is zero", "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{0.0f};
    float const Target[]{90.0f};
    float Out[]{unwritten_value};

    ml::kernel::rotate_towards_1d_degrees(Current, Target, 90.0f, default_delta_time, Out, 0);

    CHECK(Out[0] == unwritten_value);
}
