#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.subtract_scaled.Zero") {
    auto out{ml::make_vectors3f(
        TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}})};
    auto const a{ml::make_vectors3f(
        TArray<FVector3f>{{10.0f, 40.0f, 70.0f}, {20.0f, 50.0f, 80.0f}, {30.0f, 60.0f, 90.0f}})};
    auto const b{ml::make_vectors3f(
        TArray<FVector3f>{{2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}, {4.0f, 7.0f, 10.0f}})};
    auto [out_xs, out_ys, out_zs] = out.get_data();
    auto [a_xs, a_ys, a_zs] = a.get_data();
    auto [b_xs, b_ys, b_zs] = b.get_data();

    ml::kernel::subtract_scaled(out_xs,
                                out_ys,
                                out_zs,
                                a_xs,
                                a_ys,
                                a_zs,
                                b_xs,
                                b_ys,
                                b_zs,
                                0.0f,
                                out.num());

    REQUIRE(ml::almost_equal(out, a));
}

TEST_CASE("SandboxCore.Math.subtract_scaled.Half") {
    auto out{ml::make_vectors3f(
        TArray<FVector3f>{{-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}})};
    auto const a{ml::make_vectors3f(
        TArray<FVector3f>{{10.0f, 40.0f, 70.0f}, {20.0f, 50.0f, 80.0f}, {30.0f, 60.0f, 90.0f}})};
    auto const b{ml::make_vectors3f(
        TArray<FVector3f>{{2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}, {4.0f, 7.0f, 10.0f}})};
    auto [out_xs, out_ys, out_zs] = out.get_data();
    auto [a_xs, a_ys, a_zs] = a.get_data();
    auto [b_xs, b_ys, b_zs] = b.get_data();

    ml::kernel::subtract_scaled(out_xs,
                                out_ys,
                                out_zs,
                                a_xs,
                                a_ys,
                                a_zs,
                                b_xs,
                                b_ys,
                                b_zs,
                                0.5f,
                                out.num());

    auto const expected{ml::make_vectors3f(
        TArray<FVector3f>{{9.0f, 37.5f, 66.0f}, {18.5f, 47.0f, 75.5f}, {28.0f, 56.5f, 85.0f}})};
    REQUIRE(ml::almost_equal(out, expected));
}

TEST_CASE("SandboxCore.Math.subtract_scaled.Empty") {
    FVectors3f out;
    FVectors3f const a;
    FVectors3f const b;
    auto [out_xs, out_ys, out_zs] = out.get_data();
    auto [a_xs, a_ys, a_zs] = a.get_data();
    auto [b_xs, b_ys, b_zs] = b.get_data();

    ml::kernel::subtract_scaled(out_xs,
                                out_ys,
                                out_zs,
                                a_xs,
                                a_ys,
                                a_zs,
                                b_xs,
                                b_ys,
                                b_zs,
                                2.0f,
                                out.num());

    REQUIRE(ml::almost_equal(out, FVectors3f{}));
}
