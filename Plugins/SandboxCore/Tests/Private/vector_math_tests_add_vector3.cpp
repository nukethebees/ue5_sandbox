#include "SandboxCore/vector_math.h"

#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.add_vector3.adds two split vector3 arrays", "[SandboxCore][Math]") {
    TArray<float> const lhs_x{1.0f, 2.0f, 3.0f};
    TArray<float> const lhs_y{4.0f, 5.0f, 6.0f};
    TArray<float> const lhs_z{7.0f, 8.0f, 9.0f};

    TArray<float> const rhs_x{10.0f, 20.0f, 30.0f};
    TArray<float> const rhs_y{40.0f, 50.0f, 60.0f};
    TArray<float> const rhs_z{70.0f, 80.0f, 90.0f};

    TArray<float> out_x;
    TArray<float> out_y;
    TArray<float> out_z;

    out_x.SetNumUninitialized(3);
    out_y.SetNumUninitialized(3);
    out_z.SetNumUninitialized(3);

    bool const ok{
        ml::add_vector3<float>(lhs_x, lhs_y, lhs_z, rhs_x, rhs_y, rhs_z, out_x, out_y, out_z)};

    REQUIRE(ok);

    CHECK(out_x[0] == 11.0f);
    CHECK(out_x[1] == 22.0f);
    CHECK(out_x[2] == 33.0f);

    CHECK(out_y[0] == 44.0f);
    CHECK(out_y[1] == 55.0f);
    CHECK(out_y[2] == 66.0f);

    CHECK(out_z[0] == 77.0f);
    CHECK(out_z[1] == 88.0f);
    CHECK(out_z[2] == 99.0f);
}

TEST_CASE("SandboxCore.Math.add_vector3.returns false when array sizes do not match",
          "[SandboxCore][Math]") {
    TArray<float> const lhs_x{1.0f, 2.0f, 3.0f};
    TArray<float> const lhs_y{4.0f, 5.0f};
    TArray<float> const lhs_z{7.0f, 8.0f, 9.0f};

    TArray<float> const rhs_x{10.0f, 20.0f, 30.0f};
    TArray<float> const rhs_y{40.0f, 50.0f, 60.0f};
    TArray<float> const rhs_z{70.0f, 80.0f, 90.0f};

    TArray<float> out_x;
    TArray<float> out_y;
    TArray<float> out_z;

    out_x.Init(-1.0f, 3);
    out_y.Init(-1.0f, 3);
    out_z.Init(-1.0f, 3);

    bool const ok{
        ml::add_vector3<float>(lhs_x, lhs_y, lhs_z, rhs_x, rhs_y, rhs_z, out_x, out_y, out_z)};

    REQUIRE_FALSE(ok);

    CHECK(out_x[0] == -1.0f);
    CHECK(out_y[0] == -1.0f);
    CHECK(out_z[0] == -1.0f);
}
