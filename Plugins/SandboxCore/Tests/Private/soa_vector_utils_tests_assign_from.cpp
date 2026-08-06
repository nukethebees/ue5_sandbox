#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.assign_from.Vectors") {
    auto dst{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}})};
    auto const src{ml::make_vectors3f(TArray<FVector3f>{{4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f}})};

    ml::assign_from(dst, src);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign_from.Element") {
    auto dst{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, 5.0f}, {2.0f, 4.0f, 6.0f}})};
    auto const src{ml::make_vectors3f(TArray<FVector3f>{{7.0f, 8.0f, 9.0f}, {10.0f, 11.0f, 12.0f}})};

    ml::assign_from(dst, 0, src, 1);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{10.0f, 11.0f, 12.0f}, {2.0f, 4.0f, 6.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.multiply.Vectors") {
    FVectors3f dst;
    dst.set_num_uninitialised(2);

    auto const src{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, 5.0f}, {2.0f, 4.0f, 6.0f}})};
    Values const scale_factors{2.0f, 3.0f};

    ml::multiply(dst, src, scale_factors);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{2.0f, 6.0f, 10.0f}, {6.0f, 12.0f, 18.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.multiply.View") {
    FVectors3f dst;
    dst.set_num_uninitialised(1);

    auto const src{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, 5.0f}, {2.0f, 4.0f, 6.0f}})};
    Values const scale_factors{3.0f};

    ml::multiply(dst, src.get_const_view().right(1), scale_factors);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{6.0f, 12.0f, 18.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.multiply.Scalar") {
    FVectors3f dst;
    dst.set_num_uninitialised(2);

    auto const src{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, -5.0f}, {-2.0f, 4.0f, 6.0f}})};

    ml::multiply(dst, src, -2.0f);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{-2.0f, -6.0f, 10.0f}, {4.0f, -8.0f, -12.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.multiply.MutableView") {
    auto dst{ml::make_vectors3f(TArray<FVector3f>{{100.0f, 200.0f, 300.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}})};
    auto const src{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, 5.0f}, {2.0f, 4.0f, 6.0f}})};
    Values const scale_factors{2.0f, 3.0f};
    auto dst_view{dst.get_view().right(2)};

    ml::multiply(dst_view, src.get_const_view(), scale_factors);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{100.0f, 200.0f, 300.0f}, {2.0f, 6.0f, 10.0f}, {6.0f, 12.0f, 18.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.multiply.ScalarMutableView") {
    auto dst{ml::make_vectors3f(TArray<FVector3f>{{100.0f, 200.0f, 300.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}})};
    auto const src{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, 5.0f}, {2.0f, 4.0f, 6.0f}})};
    auto dst_view{dst.get_view().right(2)};

    ml::multiply(dst_view, src.get_const_view(), 2.0f);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{100.0f, 200.0f, 300.0f}, {2.0f, 6.0f, 10.0f}, {4.0f, 8.0f, 12.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}
