#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.multiply_in_place.Scalar") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    ml::multiply_in_place(vectors, 2.0f);

    auto const expected{ml::make_vectors3f({2.0f, 4.0f}, {6.0f, 8.0f}, {10.0f, 12.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.multiply_in_place.Values") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};
    Values const values{2.0f, 3.0f};

    ml::multiply_in_place(vectors, values);

    auto const expected{ml::make_vectors3f({2.0f, 6.0f}, {6.0f, 12.0f}, {10.0f, 18.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.operator_multiply_assign.Scalar") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    vectors *= 2.0f;

    auto const expected{ml::make_vectors3f({2.0f, 4.0f}, {6.0f, 8.0f}, {10.0f, 12.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}
