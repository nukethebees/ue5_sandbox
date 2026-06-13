#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.scale_vector3.Zero") {
    TArray<float> const lhs_x{1.0f, 2.0f, 3.0f};
    TArray<float> const lhs_y{4.0f, 5.0f, 6.0f};
    TArray<float> const lhs_z{7.0f, 8.0f, 9.0f};
    TArray<float> const scale_factors{0.0f, 0.0f, 0.0f};

    TArray<float> dst_x;
    TArray<float> dst_y;
    TArray<float> dst_z;
    dst_x.SetNumUninitialized(3);
    dst_y.SetNumUninitialized(3);
    dst_z.SetNumUninitialized(3);

    ml::kernel::scale_vector3(dst_x.GetData(),
                              dst_y.GetData(),
                              dst_z.GetData(),
                              lhs_x.GetData(),
                              lhs_y.GetData(),
                              lhs_z.GetData(),
                              scale_factors.GetData(),
                              lhs_x.Num());

    REQUIRE(dst_x == TArray<float>{0.0f, 0.0f, 0.0f});
    REQUIRE(dst_y == TArray<float>{0.0f, 0.0f, 0.0f});
    REQUIRE(dst_z == TArray<float>{0.0f, 0.0f, 0.0f});
}

TEST_CASE("SandboxCore.Math.scale_vector3.MixedFactors") {
    TArray<float> const lhs_x{1.0f, 2.0f, 3.0f};
    TArray<float> const lhs_y{4.0f, 5.0f, 6.0f};
    TArray<float> const lhs_z{7.0f, 8.0f, 9.0f};
    TArray<float> const scale_factors{1.0f, 2.0f, -1.0f};

    TArray<float> dst_x;
    TArray<float> dst_y;
    TArray<float> dst_z;
    dst_x.SetNumUninitialized(3);
    dst_y.SetNumUninitialized(3);
    dst_z.SetNumUninitialized(3);

    ml::kernel::scale_vector3(dst_x.GetData(),
                              dst_y.GetData(),
                              dst_z.GetData(),
                              lhs_x.GetData(),
                              lhs_y.GetData(),
                              lhs_z.GetData(),
                              scale_factors.GetData(),
                              lhs_x.Num());

    REQUIRE(dst_x == TArray<float>{1.0f, 4.0f, -3.0f});
    REQUIRE(dst_y == TArray<float>{4.0f, 10.0f, -6.0f});
    REQUIRE(dst_z == TArray<float>{7.0f, 16.0f, -9.0f});
}

TEST_CASE("SandboxCore.Math.scale_vector3.Empty") {
    TArray<float> const lhs_x{};
    TArray<float> const lhs_y{};
    TArray<float> const lhs_z{};
    TArray<float> const scale_factors{};

    TArray<float> dst_x{};
    TArray<float> dst_y{};
    TArray<float> dst_z{};

    ml::kernel::scale_vector3(dst_x.GetData(),
                              dst_y.GetData(),
                              dst_z.GetData(),
                              lhs_x.GetData(),
                              lhs_y.GetData(),
                              lhs_z.GetData(),
                              scale_factors.GetData(),
                              lhs_x.Num());

    REQUIRE(dst_x == TArray<float>{});
    REQUIRE(dst_y == TArray<float>{});
    REQUIRE(dst_z == TArray<float>{});
}
