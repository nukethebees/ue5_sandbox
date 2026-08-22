#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.multiply.Zero") {
    auto const lhs{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}})};
    TArray<float> const scale_factors{0.0f, 0.0f, 0.0f};

    FVectors3f dst;
    dst.set_num_uninitialised(lhs.num());
    auto [dst_xs, dst_ys, dst_zs] = dst.get_data();
    auto [lhs_xs, lhs_ys, lhs_zs] = lhs.get_data();

    ml::kernel::multiply(dst_xs, dst_ys, dst_zs, lhs_xs, lhs_ys, lhs_zs, scale_factors.GetData(), lhs.num());

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.Math.multiply.MixedFactors") {
    auto const lhs{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}})};
    TArray<float> const scale_factors{1.0f, 2.0f, -1.0f};

    FVectors3f dst;
    dst.set_num_uninitialised(lhs.num());
    auto [dst_xs, dst_ys, dst_zs] = dst.get_data();
    auto [lhs_xs, lhs_ys, lhs_zs] = lhs.get_data();

    ml::kernel::multiply(dst_xs, dst_ys, dst_zs, lhs_xs, lhs_ys, lhs_zs, scale_factors.GetData(), lhs.num());

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {4.0f, 10.0f, 16.0f}, {-3.0f, -6.0f, -9.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.Math.multiply.Scalar") {
    auto const lhs{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, -7.0f}, {-2.0f, 0.0f, 8.0f}, {3.0f, -6.0f, 9.0f}})};
    FVectors3f dst;
    dst.set_num_uninitialised(lhs.num());
    auto [dst_xs, dst_ys, dst_zs] = dst.get_data();
    auto [lhs_xs, lhs_ys, lhs_zs] = lhs.get_data();

    ml::kernel::multiply(dst_xs, dst_ys, dst_zs, lhs_xs, lhs_ys, lhs_zs, -2.0f, lhs.num());

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{-2.0f, -8.0f, 14.0f}, {4.0f, 0.0f, -16.0f}, {-6.0f, 12.0f, -18.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.Math.multiply.Empty") {
    FVectors3f const lhs;
    TArray<float> const scale_factors{};

    FVectors3f dst;
    auto [dst_xs, dst_ys, dst_zs] = dst.get_data();
    auto [lhs_xs, lhs_ys, lhs_zs] = lhs.get_data();

    ml::kernel::multiply(dst_xs, dst_ys, dst_zs, lhs_xs, lhs_ys, lhs_zs, scale_factors.GetData(), lhs.num());

    CHECK(ml::almost_equal(dst, FVectors3f{}));
}
