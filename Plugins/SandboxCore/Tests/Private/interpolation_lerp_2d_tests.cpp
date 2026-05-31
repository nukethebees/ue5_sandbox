#include "SandboxCore/interpolation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.Lerp2D View returns true and writes interpolated values when array "
          "sizes match",
          "[SandboxCore][Math][Lerp2D]") {
    TArray<float> const FromX{0.0f, 10.0f, -10.0f, 100.0f};
    TArray<float> const FromY{5.0f, 20.0f, -20.0f, 200.0f};

    TArray<float> const ToX{10.0f, 20.0f, 10.0f, 200.0f};
    TArray<float> const ToY{15.0f, 40.0f, 20.0f, 400.0f};

    TArray<float> const Alpha{0.0f, 0.5f, 1.0f, 0.25f};

    TArray<float> OutX;
    TArray<float> OutY;
    OutX.Init(-1.0f, FromX.Num());
    OutY.Init(-1.0f, FromY.Num());

    bool const b_result{ml::lerp_2d<float>(FromX, FromY, ToX, ToY, Alpha, OutX, OutY)};

    REQUIRE(b_result);

    CHECK(OutX[0] == 0.0f);
    CHECK(OutX[1] == 15.0f);
    CHECK(OutX[2] == 10.0f);
    CHECK(OutX[3] == 125.0f);

    CHECK(OutY[0] == 5.0f);
    CHECK(OutY[1] == 30.0f);
    CHECK(OutY[2] == 20.0f);
    CHECK(OutY[3] == 250.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D View returns true for empty arrays",
          "[SandboxCore][Math][Lerp2D]") {
    TArray<float> const FromX;
    TArray<float> const FromY;
    TArray<float> const ToX;
    TArray<float> const ToY;
    TArray<float> const Alpha;
    TArray<float> OutX;
    TArray<float> OutY;

    bool const b_result{ml::lerp_2d<float>(FromX, FromY, ToX, ToY, Alpha, OutX, OutY)};

    CHECK(b_result);
    CHECK(OutX.Num() == 0);
    CHECK(OutY.Num() == 0);
}

TEST_CASE("SandboxCore.Math.Lerp2D View returns false and leaves output unchanged when FromY has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D]") {
    TArray<float> const FromX{0.0f, 10.0f};
    TArray<float> const FromY{5.0f};
    TArray<float> const ToX{10.0f, 20.0f};
    TArray<float> const ToY{15.0f, 40.0f};
    TArray<float> const Alpha{0.5f, 0.5f};

    TArray<float> OutX;
    TArray<float> OutY;
    OutX.Init(-1.0f, FromX.Num());
    OutY.Init(-2.0f, FromX.Num());

    bool const b_result{ml::lerp_2d<float>(FromX, FromY, ToX, ToY, Alpha, OutX, OutY)};

    CHECK_FALSE(b_result);

    CHECK(OutX[0] == -1.0f);
    CHECK(OutX[1] == -1.0f);

    CHECK(OutY[0] == -2.0f);
    CHECK(OutY[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D View returns false and leaves output unchanged when ToX has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D]") {
    TArray<float> const FromX{0.0f, 10.0f};
    TArray<float> const FromY{5.0f, 20.0f};
    TArray<float> const ToX{10.0f};
    TArray<float> const ToY{15.0f, 40.0f};
    TArray<float> const Alpha{0.5f, 0.5f};

    TArray<float> OutX;
    TArray<float> OutY;
    OutX.Init(-1.0f, FromX.Num());
    OutY.Init(-2.0f, FromX.Num());

    bool const b_result{ml::lerp_2d<float>(FromX, FromY, ToX, ToY, Alpha, OutX, OutY)};

    CHECK_FALSE(b_result);

    CHECK(OutX[0] == -1.0f);
    CHECK(OutX[1] == -1.0f);

    CHECK(OutY[0] == -2.0f);
    CHECK(OutY[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D View returns false and leaves output unchanged when ToY has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D]") {
    TArray<float> const FromX{0.0f, 10.0f};
    TArray<float> const FromY{5.0f, 20.0f};
    TArray<float> const ToX{10.0f, 20.0f};
    TArray<float> const ToY{15.0f};
    TArray<float> const Alpha{0.5f, 0.5f};

    TArray<float> OutX;
    TArray<float> OutY;
    OutX.Init(-1.0f, FromX.Num());
    OutY.Init(-2.0f, FromX.Num());

    bool const b_result{ml::lerp_2d<float>(FromX, FromY, ToX, ToY, Alpha, OutX, OutY)};

    CHECK_FALSE(b_result);

    CHECK(OutX[0] == -1.0f);
    CHECK(OutX[1] == -1.0f);

    CHECK(OutY[0] == -2.0f);
    CHECK(OutY[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D View returns false and leaves output unchanged when Alpha has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D]") {
    TArray<float> const FromX{0.0f, 10.0f};
    TArray<float> const FromY{5.0f, 20.0f};
    TArray<float> const ToX{10.0f, 20.0f};
    TArray<float> const ToY{15.0f, 40.0f};
    TArray<float> const Alpha{0.5f};

    TArray<float> OutX;
    TArray<float> OutY;
    OutX.Init(-1.0f, FromX.Num());
    OutY.Init(-2.0f, FromX.Num());

    bool const b_result{ml::lerp_2d<float>(FromX, FromY, ToX, ToY, Alpha, OutX, OutY)};

    CHECK_FALSE(b_result);

    CHECK(OutX[0] == -1.0f);
    CHECK(OutX[1] == -1.0f);

    CHECK(OutY[0] == -2.0f);
    CHECK(OutY[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D View returns false and leaves output unchanged when OutX has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D]") {
    TArray<float> const FromX{0.0f, 10.0f};
    TArray<float> const FromY{5.0f, 20.0f};
    TArray<float> const ToX{10.0f, 20.0f};
    TArray<float> const ToY{15.0f, 40.0f};
    TArray<float> const Alpha{0.5f, 0.5f};

    TArray<float> OutX;
    TArray<float> OutY;
    OutX.Init(-1.0f, 1);
    OutY.Init(-2.0f, FromX.Num());

    bool const b_result{ml::lerp_2d<float>(FromX, FromY, ToX, ToY, Alpha, OutX, OutY)};

    CHECK_FALSE(b_result);

    CHECK(OutX[0] == -1.0f);

    CHECK(OutY[0] == -2.0f);
    CHECK(OutY[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D View returns false and leaves output unchanged when OutY has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D]") {
    TArray<float> const FromX{0.0f, 10.0f};
    TArray<float> const FromY{5.0f, 20.0f};
    TArray<float> const ToX{10.0f, 20.0f};
    TArray<float> const ToY{15.0f, 40.0f};
    TArray<float> const Alpha{0.5f, 0.5f};

    TArray<float> OutX;
    TArray<float> OutY;
    OutX.Init(-1.0f, FromX.Num());
    OutY.Init(-2.0f, 1);

    bool const b_result{ml::lerp_2d<float>(FromX, FromY, ToX, ToY, Alpha, OutX, OutY)};

    CHECK_FALSE(b_result);

    CHECK(OutX[0] == -1.0f);
    CHECK(OutX[1] == -1.0f);

    CHECK(OutY[0] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar returns true and writes interpolated values when array "
          "sizes match",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x{0.0f, 10.0f, -10.0f};
    TArray<float> const from_y{100.0f, 200.0f, 300.0f};

    TArray<float> const to_x{10.0f, 20.0f, 10.0f};
    TArray<float> const to_y{200.0f, 400.0f, 600.0f};

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-1.0f, from_x.Num());

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    REQUIRE(b_result);

    CHECK(out_x[0] == 5.0f);
    CHECK(out_x[1] == 15.0f);
    CHECK(out_x[2] == 0.0f);

    CHECK(out_y[0] == 150.0f);
    CHECK(out_y[1] == 300.0f);
    CHECK(out_y[2] == 450.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar supports alpha zero",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x{1.0f, 2.0f};
    TArray<float> const from_y{3.0f, 4.0f};

    TArray<float> const to_x{10.0f, 20.0f};
    TArray<float> const to_y{30.0f, 40.0f};

    float const alpha{0.0f};

    TArray<float> out_x;
    TArray<float> out_y;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-1.0f, from_x.Num());

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    REQUIRE(b_result);

    CHECK(out_x[0] == 1.0f);
    CHECK(out_x[1] == 2.0f);

    CHECK(out_y[0] == 3.0f);
    CHECK(out_y[1] == 4.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar supports alpha one",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x{1.0f, 2.0f};
    TArray<float> const from_y{3.0f, 4.0f};

    TArray<float> const to_x{10.0f, 20.0f};
    TArray<float> const to_y{30.0f, 40.0f};

    float const alpha{1.0f};

    TArray<float> out_x;
    TArray<float> out_y;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-1.0f, from_x.Num());

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    REQUIRE(b_result);

    CHECK(out_x[0] == 10.0f);
    CHECK(out_x[1] == 20.0f);

    CHECK(out_y[0] == 30.0f);
    CHECK(out_y[1] == 40.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar returns false and leaves output unchanged when FromY "
          "has the wrong size",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x{0.0f, 10.0f};
    TArray<float> const from_y{100.0f};

    TArray<float> const to_x{10.0f, 20.0f};
    TArray<float> const to_y{200.0f, 400.0f};

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-2.0f, from_x.Num());

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    CHECK_FALSE(b_result);

    CHECK(out_x[0] == -1.0f);
    CHECK(out_x[1] == -1.0f);

    CHECK(out_y[0] == -2.0f);
    CHECK(out_y[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar returns false and leaves output unchanged when ToX has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x{0.0f, 10.0f};
    TArray<float> const from_y{100.0f, 200.0f};

    TArray<float> const to_x{10.0f};
    TArray<float> const to_y{200.0f, 400.0f};

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-2.0f, from_x.Num());

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    CHECK_FALSE(b_result);

    CHECK(out_x[0] == -1.0f);
    CHECK(out_x[1] == -1.0f);

    CHECK(out_y[0] == -2.0f);
    CHECK(out_y[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar returns false and leaves output unchanged when ToY has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x{0.0f, 10.0f};
    TArray<float> const from_y{100.0f, 200.0f};

    TArray<float> const to_x{10.0f, 20.0f};
    TArray<float> const to_y{200.0f};

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-2.0f, from_x.Num());

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    CHECK_FALSE(b_result);

    CHECK(out_x[0] == -1.0f);
    CHECK(out_x[1] == -1.0f);

    CHECK(out_y[0] == -2.0f);
    CHECK(out_y[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar returns false and leaves output unchanged when OutX has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x{0.0f, 10.0f};
    TArray<float> const from_y{100.0f, 200.0f};

    TArray<float> const to_x{10.0f, 20.0f};
    TArray<float> const to_y{200.0f, 400.0f};

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;
    out_x.Init(-1.0f, 1);
    out_y.Init(-2.0f, from_x.Num());

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    CHECK_FALSE(b_result);

    CHECK(out_x[0] == -1.0f);

    CHECK(out_y[0] == -2.0f);
    CHECK(out_y[1] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar returns false and leaves output unchanged when OutY has "
          "the wrong size",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x{0.0f, 10.0f};
    TArray<float> const from_y{100.0f, 200.0f};

    TArray<float> const to_x{10.0f, 20.0f};
    TArray<float> const to_y{200.0f, 400.0f};

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;
    out_x.Init(-1.0f, from_x.Num());
    out_y.Init(-2.0f, 1);

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    CHECK_FALSE(b_result);

    CHECK(out_x[0] == -1.0f);
    CHECK(out_x[1] == -1.0f);

    CHECK(out_y[0] == -2.0f);
}

TEST_CASE("SandboxCore.Math.Lerp2D Scalar returns true for empty arrays",
          "[SandboxCore][Math][Lerp2D][Scalar]") {
    TArray<float> const from_x;
    TArray<float> const from_y;
    TArray<float> const to_x;
    TArray<float> const to_y;

    float const alpha{0.5f};

    TArray<float> out_x;
    TArray<float> out_y;

    bool const b_result{ml::lerp_2d<float>(from_x, from_y, to_x, to_y, alpha, out_x, out_y)};

    CHECK(b_result);
    CHECK(out_x.Num() == 0);
    CHECK(out_y.Num() == 0);
}
