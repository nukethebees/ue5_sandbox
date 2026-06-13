#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.add_scaled_in_place.Zero") {
    TArray<float> dst_x{1.0f, 2.0f, 3.0f};
    TArray<float> dst_y{4.0f, 5.0f, 6.0f};
    TArray<float> dst_z{7.0f, 8.0f, 9.0f};

    TArray<float> const src_x{10.0f, 20.0f, 30.0f};
    TArray<float> const src_y{40.0f, 50.0f, 60.0f};
    TArray<float> const src_z{70.0f, 80.0f, 90.0f};

    ml::kernel::add_scaled_in_place(dst_x.GetData(),
                                    dst_y.GetData(),
                                    dst_z.GetData(),
                                    src_x.GetData(),
                                    src_y.GetData(),
                                    src_z.GetData(),
                                    0.0f,
                                    dst_x.Num());

    REQUIRE(dst_x == TArray<float>{1.0f, 2.0f, 3.0f});
    REQUIRE(dst_y == TArray<float>{4.0f, 5.0f, 6.0f});
    REQUIRE(dst_z == TArray<float>{7.0f, 8.0f, 9.0f});
}

TEST_CASE("SandboxCore.Math.add_scaled_in_place.Two") {
    TArray<float> dst_x{1.0f, 2.0f, 3.0f};
    TArray<float> dst_y{4.0f, 5.0f, 6.0f};
    TArray<float> dst_z{7.0f, 8.0f, 9.0f};

    TArray<float> const src_x{10.0f, 20.0f, 30.0f};
    TArray<float> const src_y{40.0f, 50.0f, 60.0f};
    TArray<float> const src_z{70.0f, 80.0f, 90.0f};

    ml::kernel::add_scaled_in_place(dst_x.GetData(),
                                    dst_y.GetData(),
                                    dst_z.GetData(),
                                    src_x.GetData(),
                                    src_y.GetData(),
                                    src_z.GetData(),
                                    2.0f,
                                    dst_x.Num());

    REQUIRE(dst_x == TArray<float>{21.0f, 42.0f, 63.0f});
    REQUIRE(dst_y == TArray<float>{84.0f, 105.0f, 126.0f});
    REQUIRE(dst_z == TArray<float>{147.0f, 168.0f, 189.0f});
}

TEST_CASE("SandboxCore.Math.add_scaled_in_place.Empty") {
    TArray<float> dst_x{};
    TArray<float> dst_y{};
    TArray<float> dst_z{};

    TArray<float> const src_x{};
    TArray<float> const src_y{};
    TArray<float> const src_z{};

    ml::kernel::add_scaled_in_place(dst_x.GetData(),
                                    dst_y.GetData(),
                                    dst_z.GetData(),
                                    src_x.GetData(),
                                    src_y.GetData(),
                                    src_z.GetData(),
                                    2.0f,
                                    dst_x.Num());

    REQUIRE(dst_x == TArray<float>{});
    REQUIRE(dst_y == TArray<float>{});
    REQUIRE(dst_z == TArray<float>{});
}
