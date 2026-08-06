#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.multiply_in_place.Scalar") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{-2.0f, 4.0f, 0.0f}, {3.0f, -5.0f, 6.0f}})};
    auto [xs, ys, zs] = vectors.get_data();

    ml::kernel::multiply_in_place(xs, ys, zs, -2.0f, vectors.num());

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{4.0f, -8.0f, 0.0f}, {-6.0f, 10.0f, -12.0f}})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.Math.multiply_in_place.SharedFactors") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}})};
    TArray<float> const factors{0.0f, -1.0f, 2.0f};
    auto [xs, ys, zs] = vectors.get_data();

    ml::kernel::multiply_in_place(xs, ys, zs, factors.GetData(), vectors.num());

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {-2.0f, -5.0f, -8.0f}, {6.0f, 12.0f, 18.0f}})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.Math.multiply_in_place.ComponentFactors") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, 5.0f}, {2.0f, 4.0f, 6.0f}})};
    auto const factors{ml::make_vectors3f(TArray<FVector3f>{{2.0f, -1.0f, 0.5f}, {3.0f, 0.0f, -2.0f}})};
    auto [xs, ys, zs] = vectors.get_data();
    auto [factor_xs, factor_ys, factor_zs] = factors.get_data();

    ml::kernel::multiply_in_place(xs, ys, zs, factor_xs, factor_ys, factor_zs, vectors.num());

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{2.0f, -3.0f, 2.5f}, {6.0f, 0.0f, -12.0f}})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.Math.multiply_in_place.Empty") {
    FVectors3f vectors;
    auto [xs, ys, zs] = vectors.get_data();

    ml::kernel::multiply_in_place(xs, ys, zs, 2.0f, vectors.num());

    CHECK(ml::almost_equal(vectors, FVectors3f{}));
}
