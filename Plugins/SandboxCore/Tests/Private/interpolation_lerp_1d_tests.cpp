#include "SandboxCore/interpolation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.Lerp1D.View writes interpolated values",
          "[SandboxCore][Math][Lerp1D]") {
    TArray<float> const from{0.0f, 10.0f, -10.0f, 100.0f};
    TArray<float> const to{10.0f, 20.0f, 10.0f, 200.0f};
    TArray<float> const alpha{0.0f, 0.5f, 1.0f, 0.25f};
    TArray<float> out;
    out.Init(-1.0f, from.Num());

    ml::lerp_1d<float>(out, from, to, alpha);

    CHECK(out[0] == 0.0f);
    CHECK(out[1] == 15.0f);
    CHECK(out[2] == 10.0f);
    CHECK(out[3] == 125.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D.View supports empty arrays", "[SandboxCore][Math][Lerp1D]") {
    TArray<float> const from;
    TArray<float> const to;
    TArray<float> const alpha;
    TArray<float> out;

    ml::lerp_1d<float>(out, from, to, alpha);

    CHECK(out.Num() == 0);
}

TEST_CASE("SandboxCore.Math.Lerp1D.Scalar writes interpolated values",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from{0.0f, 10.0f, -10.0f, 100.0f};
    TArray<float> const to{10.0f, 20.0f, 10.0f, 200.0f};
    float const alpha{0.5f};

    TArray<float> out;
    out.Init(-1.0f, from.Num());

    ml::lerp_1d<float>(out, from, to, alpha);

    CHECK(out[0] == 5.0f);
    CHECK(out[1] == 15.0f);
    CHECK(out[2] == 0.0f);
    CHECK(out[3] == 150.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D.Scalar supports alpha zero",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from{1.0f, 2.0f, 3.0f};
    TArray<float> const to{10.0f, 20.0f, 30.0f};
    float const alpha{0.0f};

    TArray<float> out;
    out.Init(-1.0f, from.Num());

    ml::lerp_1d<float>(out, from, to, alpha);

    CHECK(out[0] == 1.0f);
    CHECK(out[1] == 2.0f);
    CHECK(out[2] == 3.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D.Scalar supports alpha one",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from{1.0f, 2.0f, 3.0f};
    TArray<float> const to{10.0f, 20.0f, 30.0f};
    float const alpha{1.0f};

    TArray<float> out;
    out.Init(-1.0f, from.Num());

    ml::lerp_1d<float>(out, from, to, alpha);

    CHECK(out[0] == 10.0f);
    CHECK(out[1] == 20.0f);
    CHECK(out[2] == 30.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D.Scalar supports empty arrays",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from;
    TArray<float> const to;
    float const alpha{0.5f};
    TArray<float> out;

    ml::lerp_1d<float>(out, from, to, alpha);

    CHECK(out.Num() == 0);
}
