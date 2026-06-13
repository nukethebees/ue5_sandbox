#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.add_vector3_in_place.Zero") {
    TArray<float> dst_x{1.0f, 2.0f, 3.0f};
    TArray<float> dst_y{4.0f, 5.0f, 6.0f};
    TArray<float> dst_z{7.0f, 8.0f, 9.0f};

    TArray<float> const src_x{0.0f, 0.0f, 0.0f};
    TArray<float> const src_y{0.0f, 0.0f, 0.0f};
    TArray<float> const src_z{0.0f, 0.0f, 0.0f};

    ml::kernel::add_vector3_in_place(dst_x.GetData(),
                                     dst_y.GetData(),
                                     dst_z.GetData(),
                                     src_x.GetData(),
                                     src_y.GetData(),
                                     src_z.GetData(),
                                     dst_x.Num());

    REQUIRE(dst_x == TArray<float>{1.0f, 2.0f, 3.0f});
    REQUIRE(dst_y == TArray<float>{4.0f, 5.0f, 6.0f});
    REQUIRE(dst_z == TArray<float>{7.0f, 8.0f, 9.0f});
}

TEST_CASE("SandboxCore.Math.add_vector3_in_place.One") {
    TArray<float> dst_x{1.0f, 2.0f, 3.0f};
    TArray<float> dst_y{4.0f, 5.0f, 6.0f};
    TArray<float> dst_z{7.0f, 8.0f, 9.0f};

    TArray<float> const src_x{1.0f, 1.0f, 1.0f};
    TArray<float> const src_y{1.0f, 1.0f, 1.0f};
    TArray<float> const src_z{1.0f, 1.0f, 1.0f};

    ml::kernel::add_vector3_in_place(dst_x.GetData(),
                                     dst_y.GetData(),
                                     dst_z.GetData(),
                                     src_x.GetData(),
                                     src_y.GetData(),
                                     src_z.GetData(),
                                     dst_x.Num());

    REQUIRE(dst_x == TArray<float>{2.0f, 3.0f, 4.0f});
    REQUIRE(dst_y == TArray<float>{5.0f, 6.0f, 7.0f});
    REQUIRE(dst_z == TArray<float>{8.0f, 9.0f, 10.0f});
}

TEST_CASE("SandboxCore.Math.add_vector3_in_place.Empty") {
    TArray<float> dst_x{};
    TArray<float> dst_y{};
    TArray<float> dst_z{};

    TArray<float> const src_x{};
    TArray<float> const src_y{};
    TArray<float> const src_z{};

    ml::kernel::add_vector3_in_place(dst_x.GetData(),
                                     dst_y.GetData(),
                                     dst_z.GetData(),
                                     src_x.GetData(),
                                     src_y.GetData(),
                                     src_z.GetData(),
                                     dst_x.Num());

    REQUIRE(dst_x == TArray<float>{});
    REQUIRE(dst_y == TArray<float>{});
    REQUIRE(dst_z == TArray<float>{});
}
