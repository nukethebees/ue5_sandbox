#include "SandboxCore/interpolation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.Lerp3D.View writes interpolated values",
          "[SandboxCore][Math][Lerp3D]") {
    TArray<float> const from_x{0.0f, 10.0f, -10.0f, 100.0f};
    TArray<float> const from_y{5.0f, 20.0f, -20.0f, 200.0f};
    TArray<float> const from_z{50.0f, 100.0f, -100.0f, 1000.0f};

    TArray<float> const to_x{10.0f, 20.0f, 10.0f, 200.0f};
    TArray<float> const to_y{15.0f, 40.0f, 20.0f, 400.0f};
    TArray<float> const to_z{60.0f, 200.0f, 100.0f, 2000.0f};

    TArray<float> const alpha{0.0f, 0.5f, 1.0f, 0.25f};

    TArray<float> out_x;
    TArray<float> out_y;
    TArray<float> out_z;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-2.0f, from_y.Num());
    out_z.Init(-3.0f, from_z.Num());

    ml::lerp_3d<float>(out_x, out_y, out_z, from_x, from_y, from_z, to_x, to_y, to_z, alpha);

    CHECK(out_x[0] == 0.0f);
    CHECK(out_x[1] == 15.0f);
    CHECK(out_x[2] == 10.0f);
    CHECK(out_x[3] == 125.0f);

    CHECK(out_y[0] == 5.0f);
    CHECK(out_y[1] == 30.0f);
    CHECK(out_y[2] == 20.0f);
    CHECK(out_y[3] == 250.0f);

    CHECK(out_z[0] == 50.0f);
    CHECK(out_z[1] == 150.0f);
    CHECK(out_z[2] == 100.0f);
    CHECK(out_z[3] == 1250.0f);
}

TEST_CASE("SandboxCore.Math.Lerp3D.View supports empty arrays", "[SandboxCore][Math][Lerp3D]") {
    TArray<float> const from_x;
    TArray<float> const from_y;
    TArray<float> const from_z;
    TArray<float> const to_x;
    TArray<float> const to_y;
    TArray<float> const to_z;
    TArray<float> const alpha;
    TArray<float> out_x;
    TArray<float> out_y;
    TArray<float> out_z;

    ml::lerp_3d<float>(out_x, out_y, out_z, from_x, from_y, from_z, to_x, to_y, to_z, alpha);

    CHECK(out_x.Num() == 0);
    CHECK(out_y.Num() == 0);
    CHECK(out_z.Num() == 0);
}

TEST_CASE("SandboxCore.Math.Lerp3D.Scalar writes interpolated values",
          "[SandboxCore][Math][Lerp3D][Scalar]") {
    TArray<float> const from_x{0.0f, 10.0f, -10.0f};
    TArray<float> const from_y{100.0f, 200.0f, 300.0f};
    TArray<float> const from_z{1000.0f, 2000.0f, 3000.0f};

    TArray<float> const to_x{10.0f, 20.0f, 10.0f};
    TArray<float> const to_y{200.0f, 400.0f, 600.0f};
    TArray<float> const to_z{2000.0f, 4000.0f, 6000.0f};

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;
    TArray<float> out_z;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-2.0f, from_x.Num());
    out_z.Init(-3.0f, from_x.Num());

    ml::lerp_3d<float>(out_x, out_y, out_z, from_x, from_y, from_z, to_x, to_y, to_z, alpha);

    CHECK(out_x[0] == 5.0f);
    CHECK(out_x[1] == 15.0f);
    CHECK(out_x[2] == 0.0f);

    CHECK(out_y[0] == 150.0f);
    CHECK(out_y[1] == 300.0f);
    CHECK(out_y[2] == 450.0f);

    CHECK(out_z[0] == 1500.0f);
    CHECK(out_z[1] == 3000.0f);
    CHECK(out_z[2] == 4500.0f);
}

TEST_CASE("SandboxCore.Math.Lerp3D.Scalar supports alpha zero",
          "[SandboxCore][Math][Lerp3D][Scalar]") {
    TArray<float> const from_x{1.0f, 2.0f};
    TArray<float> const from_y{3.0f, 4.0f};
    TArray<float> const from_z{5.0f, 6.0f};

    TArray<float> const to_x{10.0f, 20.0f};
    TArray<float> const to_y{30.0f, 40.0f};
    TArray<float> const to_z{50.0f, 60.0f};

    float const alpha{0.0f};

    TArray<float> out_x;
    TArray<float> out_y;
    TArray<float> out_z;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-2.0f, from_x.Num());
    out_z.Init(-3.0f, from_x.Num());

    ml::lerp_3d<float>(out_x, out_y, out_z, from_x, from_y, from_z, to_x, to_y, to_z, alpha);

    CHECK(out_x[0] == 1.0f);
    CHECK(out_x[1] == 2.0f);

    CHECK(out_y[0] == 3.0f);
    CHECK(out_y[1] == 4.0f);

    CHECK(out_z[0] == 5.0f);
    CHECK(out_z[1] == 6.0f);
}

TEST_CASE("SandboxCore.Math.Lerp3D.Scalar supports alpha one",
          "[SandboxCore][Math][Lerp3D][Scalar]") {
    TArray<float> const from_x{1.0f, 2.0f};
    TArray<float> const from_y{3.0f, 4.0f};
    TArray<float> const from_z{5.0f, 6.0f};

    TArray<float> const to_x{10.0f, 20.0f};
    TArray<float> const to_y{30.0f, 40.0f};
    TArray<float> const to_z{50.0f, 60.0f};

    float const alpha{1.0f};

    TArray<float> out_x;
    TArray<float> out_y;
    TArray<float> out_z;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-2.0f, from_x.Num());
    out_z.Init(-3.0f, from_x.Num());

    ml::lerp_3d<float>(out_x, out_y, out_z, from_x, from_y, from_z, to_x, to_y, to_z, alpha);

    CHECK(out_x[0] == 10.0f);
    CHECK(out_x[1] == 20.0f);

    CHECK(out_y[0] == 30.0f);
    CHECK(out_y[1] == 40.0f);

    CHECK(out_z[0] == 50.0f);
    CHECK(out_z[1] == 60.0f);
}

TEST_CASE("SandboxCore.Math.Lerp3D.Scalar supports empty arrays",
          "[SandboxCore][Math][Lerp3D][Scalar]") {
    TArray<float> const from_x;
    TArray<float> const from_y;
    TArray<float> const from_z;

    TArray<float> const to_x;
    TArray<float> const to_y;
    TArray<float> const to_z;

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;
    TArray<float> out_z;

    ml::lerp_3d<float>(out_x, out_y, out_z, from_x, from_y, from_z, to_x, to_y, to_z, alpha);

    CHECK(out_x.Num() == 0);
    CHECK(out_y.Num() == 0);
    CHECK(out_z.Num() == 0);
}
