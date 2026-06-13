#include "SandboxCore/interpolation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.Lerp1DInPlace.View writes interpolated values",
          "[SandboxCore][Math][Lerp1D][InPlace]") {
    TArray<float> current{0.0f, 10.0f, 20.0f};
    TArray<float> const target{100.0f, 110.0f, 120.0f};
    TArray<float> const alpha{0.0f, 0.5f, 1.0f};

    ml::lerp_1d_in_place<float>(current, target, alpha);

    CHECK(current[0] == 0.0f);
    CHECK(current[1] == 60.0f);
    CHECK(current[2] == 120.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1DInPlace.Scalar writes interpolated values",
          "[SandboxCore][Math][Lerp1D][InPlace][Scalar]") {
    TArray<float> current{0.0f, 10.0f};
    TArray<float> const target{100.0f, 110.0f};

    ml::lerp_1d_in_place<float>(current, target, 0.25f);

    CHECK(current[0] == 25.0f);
    CHECK(current[1] == 35.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2DInPlace.View writes interpolated values",
          "[SandboxCore][Math][Lerp2D][InPlace]") {
    TArray<float> current_x{0.0f, 10.0f, 20.0f};
    TArray<float> current_y{30.0f, 40.0f, 50.0f};

    TArray<float> const target_x{100.0f, 110.0f, 120.0f};
    TArray<float> const target_y{130.0f, 140.0f, 150.0f};
    TArray<float> const alpha{0.0f, 0.5f, 1.0f};

    ml::lerp_2d_in_place<float>(current_x, current_y, target_x, target_y, alpha);

    CHECK(current_x[0] == 0.0f);
    CHECK(current_x[1] == 60.0f);
    CHECK(current_x[2] == 120.0f);

    CHECK(current_y[0] == 30.0f);
    CHECK(current_y[1] == 90.0f);
    CHECK(current_y[2] == 150.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2DInPlace.Scalar writes interpolated values",
          "[SandboxCore][Math][Lerp2D][InPlace][Scalar]") {
    TArray<float> current_x{0.0f, 10.0f};
    TArray<float> current_y{20.0f, 30.0f};

    TArray<float> const target_x{100.0f, 110.0f};
    TArray<float> const target_y{120.0f, 130.0f};

    ml::lerp_2d_in_place<float>(current_x, current_y, target_x, target_y, 0.25f);

    CHECK(current_x[0] == 25.0f);
    CHECK(current_x[1] == 35.0f);

    CHECK(current_y[0] == 45.0f);
    CHECK(current_y[1] == 55.0f);
}

TEST_CASE("SandboxCore.Math.Lerp3DInPlace.View writes interpolated values",
          "[SandboxCore][Math][Lerp3D][InPlace]") {
    TArray<float> current_x{0.0f, 10.0f, 20.0f};
    TArray<float> current_y{30.0f, 40.0f, 50.0f};
    TArray<float> current_z{60.0f, 70.0f, 80.0f};

    TArray<float> const target_x{100.0f, 110.0f, 120.0f};
    TArray<float> const target_y{130.0f, 140.0f, 150.0f};
    TArray<float> const target_z{160.0f, 170.0f, 180.0f};
    TArray<float> const alpha{0.0f, 0.5f, 1.0f};

    ml::lerp_3d_in_place<float>(
        current_x, current_y, current_z, target_x, target_y, target_z, alpha);

    CHECK(current_x[0] == 0.0f);
    CHECK(current_x[1] == 60.0f);
    CHECK(current_x[2] == 120.0f);

    CHECK(current_y[0] == 30.0f);
    CHECK(current_y[1] == 90.0f);
    CHECK(current_y[2] == 150.0f);

    CHECK(current_z[0] == 60.0f);
    CHECK(current_z[1] == 120.0f);
    CHECK(current_z[2] == 180.0f);
}

TEST_CASE("SandboxCore.Math.Lerp3DInPlace.Scalar writes interpolated values",
          "[SandboxCore][Math][Lerp3D][InPlace][Scalar]") {
    TArray<float> current_x{0.0f, 10.0f};
    TArray<float> current_y{20.0f, 30.0f};
    TArray<float> current_z{40.0f, 50.0f};

    TArray<float> const target_x{100.0f, 110.0f};
    TArray<float> const target_y{120.0f, 130.0f};
    TArray<float> const target_z{140.0f, 150.0f};

    ml::lerp_3d_in_place<float>(
        current_x, current_y, current_z, target_x, target_y, target_z, 0.25f);

    CHECK(current_x[0] == 25.0f);
    CHECK(current_x[1] == 35.0f);

    CHECK(current_y[0] == 45.0f);
    CHECK(current_y[1] == 55.0f);

    CHECK(current_z[0] == 65.0f);
    CHECK(current_z[1] == 75.0f);
}
