#include "SandboxCore/Public/interpolation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.Lerp1D View returns true and writes interpolated values when array "
          "sizes match",
          "[SandboxCore][Math][Lerp1D]") {
    TArray<float> const From{0.0f, 10.0f, -10.0f, 100.0f};
    TArray<float> const To{10.0f, 20.0f, 10.0f, 200.0f};
    TArray<float> const Alpha{0.0f, 0.5f, 1.0f, 0.25f};
    TArray<float> Out;
    Out.Init(-1.0f, From.Num());

    bool const b_result{ml::lerp_1d<float>(From, To, Alpha, Out)};

    REQUIRE(b_result);
    CHECK(Out[0] == 0.0f);
    CHECK(Out[1] == 15.0f);
    CHECK(Out[2] == 10.0f);
    CHECK(Out[3] == 125.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D View returns false and leaves output unchanged when To has the "
          "wrong size",
          "[SandboxCore][Math][Lerp1D]") {
    TArray<float> const From{0.0f, 10.0f};
    TArray<float> const To{10.0f};
    TArray<float> const Alpha{0.5f, 0.5f};
    TArray<float> Out;
    Out.Init(-1.0f, From.Num());

    bool const b_result{ml::lerp_1d<float>(From, To, Alpha, Out)};

    CHECK_FALSE(b_result);
    CHECK(Out[0] == -1.0f);
    CHECK(Out[1] == -1.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D View returns false and leaves output unchanged when Alpha has "
          "the wrong size",
          "[SandboxCore][Math][Lerp1D]") {
    TArray<float> const From{0.0f, 10.0f};
    TArray<float> const To{10.0f, 20.0f};
    TArray<float> const Alpha{0.5f};
    TArray<float> Out;
    Out.Init(-1.0f, From.Num());

    bool const b_result{ml::lerp_1d<float>(From, To, Alpha, Out)};

    CHECK_FALSE(b_result);
    CHECK(Out[0] == -1.0f);
    CHECK(Out[1] == -1.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D View returns false and leaves output unchanged when Out has the "
          "wrong size",
          "[SandboxCore][Math][Lerp1D]") {
    TArray<float> const From{0.0f, 10.0f};
    TArray<float> const To{10.0f, 20.0f};
    TArray<float> const Alpha{0.5f, 0.5f};
    TArray<float> Out;
    Out.Init(-1.0f, 1);

    bool const b_result{ml::lerp_1d<float>(From, To, Alpha, Out)};

    CHECK_FALSE(b_result);
    CHECK(Out[0] == -1.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D Scalar returns true and writes interpolated values when array "
          "sizes match",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from{0.0f, 10.0f, -10.0f, 100.0f};
    TArray<float> const to{10.0f, 20.0f, 10.0f, 200.0f};
    float const alpha{0.5f};

    TArray<float> out;
    out.Init(-1.0f, from.Num());

    bool const b_result{ml::lerp_1d<float>(from, to, alpha, out)};

    REQUIRE(b_result);
    CHECK(out[0] == 5.0f);
    CHECK(out[1] == 15.0f);
    CHECK(out[2] == 0.0f);
    CHECK(out[3] == 150.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D Scalar supports alpha zero",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from{1.0f, 2.0f, 3.0f};
    TArray<float> const to{10.0f, 20.0f, 30.0f};
    float const alpha{0.0f};

    TArray<float> out;
    out.Init(-1.0f, from.Num());

    bool const b_result{ml::lerp_1d<float>(from, to, alpha, out)};

    REQUIRE(b_result);
    CHECK(out[0] == 1.0f);
    CHECK(out[1] == 2.0f);
    CHECK(out[2] == 3.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D Scalar supports alpha one",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from{1.0f, 2.0f, 3.0f};
    TArray<float> const to{10.0f, 20.0f, 30.0f};
    float const alpha{1.0f};

    TArray<float> out;
    out.Init(-1.0f, from.Num());

    bool const b_result{ml::lerp_1d<float>(from, to, alpha, out)};

    REQUIRE(b_result);
    CHECK(out[0] == 10.0f);
    CHECK(out[1] == 20.0f);
    CHECK(out[2] == 30.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D Scalar returns false and leaves output unchanged when To has "
          "the wrong size",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from{0.0f, 10.0f};
    TArray<float> const to{10.0f};
    float const alpha{0.5f};

    TArray<float> out;
    out.Init(-1.0f, from.Num());

    bool const b_result{ml::lerp_1d<float>(from, to, alpha, out)};

    CHECK_FALSE(b_result);
    CHECK(out[0] == -1.0f);
    CHECK(out[1] == -1.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D Scalar returns false and leaves output unchanged when Out has "
          "the wrong size",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from{0.0f, 10.0f};
    TArray<float> const to{10.0f, 20.0f};
    float const alpha{0.5f};

    TArray<float> out;
    out.Init(-1.0f, 1);

    bool const b_result{ml::lerp_1d<float>(from, to, alpha, out)};

    CHECK_FALSE(b_result);
    CHECK(out[0] == -1.0f);
}

TEST_CASE("SandboxCore.Math.Lerp1D Scalar returns true for empty arrays",
          "[SandboxCore][Math][Lerp1D][Scalar]") {
    TArray<float> const from;
    TArray<float> const to;
    float const alpha{0.5f};
    TArray<float> out;

    bool const b_result{ml::lerp_1d<float>(from, to, alpha, out)};

    CHECK(b_result);
    CHECK(out.Num() == 0);
}
