#include "SandboxCore/Public/rotation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.RotateTowards1D returns current angle when target is equal",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{0.0f, 90.0f, 360.0f};
    float const Target[]{0.0f, 90.0f, 0.0f};
    float Out[]{-1.0f, -1.0f, -1.0f};

    ml::kernel::rotate_towards_1d(Current, Target, 90.0f, 1.0f, Out, 3);

    CHECK(Out[0] == 0.0f);
    CHECK(Out[1] == 90.0f);
    CHECK(Out[2] == 360.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D rotates positively when target is clockwise",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{0.0f};
    float const Target[]{90.0f};
    float Out[]{-1.0f};

    ml::kernel::rotate_towards_1d(Current, Target, 30.0f, 1.0f, Out, 1);

    CHECK(Out[0] == 30.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D rotates negatively when target is anticlockwise",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{90.0f};
    float const Target[]{0.0f};
    float Out[]{-1.0f};

    ml::kernel::rotate_towards_1d(Current, Target, 30.0f, 1.0f, Out, 1);

    CHECK(Out[0] == 60.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D clamps to target when positive step would overshoot",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{0.0f};
    float const Target[]{20.0f};
    float Out[]{-1.0f};

    constexpr float speed{90.f};

    ml::kernel::rotate_towards_1d(Current, Target, speed, 1.0f, Out, 1);

    CHECK(Out[0] == 20.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D clamps to target when negative step would overshoot",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{20.0f};
    float const Target[]{0.0f};
    float Out[]{-1.0f};

    constexpr float speed{90.f};
    ml::kernel::rotate_towards_1d(Current, Target, speed, 1.0f, Out, 1);

    CHECK(Out[0] == 0.0f);
}

TEST_CASE(
    "SandboxCore.Math.RotateTowards1D uses shortest path across zero when rotating positively",
    "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{350.0f};
    float const Target[]{10.0f};
    float Out[]{-1.0f};

    constexpr float speed{5.0f};
    ml::kernel::rotate_towards_1d(Current, Target, speed, 1.0f, Out, 1);

    CHECK(Out[0] == 355.0f);
}

TEST_CASE(
    "SandboxCore.Math.RotateTowards1D uses shortest path across zero when rotating negatively",
    "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{10.0f};
    float const Target[]{350.0f};
    float Out[]{-1.0f};

    ml::kernel::rotate_towards_1d(Current, Target, 5.0f, 1.0f, Out, 1);

    CHECK(Out[0] == 5.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D clamps to wrapped target when crossing zero positively",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{350.0f};
    float const Target[]{10.0f};
    float Out[]{-1.0f};

    ml::kernel::rotate_towards_1d(Current, Target, 90.0f, 1.0f, Out, 1);

    CHECK(Out[0] == 10.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D clamps to wrapped target when crossing zero negatively",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{10.0f};
    float const Target[]{350.0f};
    float Out[]{-1.0f};

    constexpr float speed{90.f};
    ml::kernel::rotate_towards_1d(Current, Target, speed, 1.0f, Out, 1);

    CHECK(Out[0] == 350.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D scales step by delta time",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{0.0f};
    float const Target[]{90.0f};
    float Out[]{-1.0f};

    ml::kernel::rotate_towards_1d(Current, Target, 60.0f, 0.5f, Out, 1);

    CHECK(Out[0] == 30.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D writes multiple rotated angles",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{0.0f, 90.0f, 350.0f, 10.0f};
    float const Target[]{90.0f, 0.0f, 10.0f, 350.0f};
    float Out[]{-1.0f, -1.0f, -1.0f, -1.0f};

    ml::kernel::rotate_towards_1d(Current, Target, 30.0f, 1.0f, Out, 4);

    CHECK(Out[0] == 30.0f);
    CHECK(Out[1] == 60.0f);
    CHECK(Out[2] == 10.0f);
    CHECK(Out[3] == 350.0f);
}

TEST_CASE("SandboxCore.Math.RotateTowards1D does not write output when count is zero",
          "[SandboxCore][Math][RotateTowards1D]") {
    float const Current[]{0.0f};
    float const Target[]{90.0f};
    float Out[]{-1.0f};

    ml::kernel::rotate_towards_1d(Current, Target, 90.0f, 1.0f, Out, 0);

    CHECK(Out[0] == -1.0f);
}
