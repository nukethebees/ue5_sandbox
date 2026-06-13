#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.multiply_in_place.Scalar") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 2.0f};
    vectors.ys = Values{3.0f, 4.0f};
    vectors.zs = Values{5.0f, 6.0f};

    ml::multiply_in_place(vectors, 2.0f);

    Values const expected_xs{2.0f, 4.0f};
    Values const expected_ys{6.0f, 8.0f};
    Values const expected_zs{10.0f, 12.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.multiply_in_place.Values") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 2.0f};
    vectors.ys = Values{3.0f, 4.0f};
    vectors.zs = Values{5.0f, 6.0f};
    Values const values{2.0f, 3.0f};

    ml::multiply_in_place(vectors, values);

    Values const expected_xs{2.0f, 6.0f};
    Values const expected_ys{6.0f, 12.0f};
    Values const expected_zs{10.0f, 18.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.operator_multiply_assign.Scalar") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 2.0f};
    vectors.ys = Values{3.0f, 4.0f};
    vectors.zs = Values{5.0f, 6.0f};

    vectors *= 2.0f;

    Values const expected_xs{2.0f, 4.0f};
    Values const expected_ys{6.0f, 8.0f};
    Values const expected_zs{10.0f, 12.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}
