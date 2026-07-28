#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.multiply_in_place.Scalar") {
    auto vectors{ml::make_vectors3f({-2.0f, 3.0f}, {4.0f, -5.0f}, {0.0f, 6.0f})};

    ml::kernel::multiply_in_place(vectors.xs.GetData(),
                                  vectors.ys.GetData(),
                                  vectors.zs.GetData(),
                                  -2.0f,
                                  vectors.num());

    auto const expected{
        ml::make_vectors3f({4.0f, -6.0f}, {-8.0f, 10.0f}, {0.0f, -12.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.Math.multiply_in_place.SharedFactors") {
    auto vectors{
        ml::make_vectors3f({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f})};
    TArray<float> const factors{0.0f, -1.0f, 2.0f};

    ml::kernel::multiply_in_place(vectors.xs.GetData(),
                                  vectors.ys.GetData(),
                                  vectors.zs.GetData(),
                                  factors.GetData(),
                                  vectors.num());

    auto const expected{
        ml::make_vectors3f({0.0f, -2.0f, 6.0f}, {0.0f, -5.0f, 12.0f}, {0.0f, -8.0f, 18.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.Math.multiply_in_place.ComponentFactors") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};
    auto const factors{ml::make_vectors3f({2.0f, 3.0f}, {-1.0f, 0.0f}, {0.5f, -2.0f})};

    ml::kernel::multiply_in_place(vectors.xs.GetData(),
                                  vectors.ys.GetData(),
                                  vectors.zs.GetData(),
                                  factors.xs.GetData(),
                                  factors.ys.GetData(),
                                  factors.zs.GetData(),
                                  vectors.num());

    auto const expected{
        ml::make_vectors3f({2.0f, 6.0f}, {-3.0f, 0.0f}, {2.5f, -12.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.Math.multiply_in_place.Empty") {
    FVectors3f vectors;

    ml::kernel::multiply_in_place(vectors.xs.GetData(),
                                  vectors.ys.GetData(),
                                  vectors.zs.GetData(),
                                  2.0f,
                                  vectors.num());

    CHECK(ml::almost_equal(vectors, FVectors3f{}));
}
