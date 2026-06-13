#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.fill.Vectors") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 2.0f};
    vectors.ys = Values{3.0f, 4.0f};
    vectors.zs = Values{5.0f, 6.0f};

    ml::fill(vectors, 7.0f);

    Values const expected{7.0f, 7.0f};

    CHECK(vectors.xs == expected);
    CHECK(vectors.ys == expected);
    CHECK(vectors.zs == expected);
}

TEST_CASE("SandboxCore.SoaVectorUtils.fill.View") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 2.0f, 3.0f};
    vectors.ys = Values{4.0f, 5.0f, 6.0f};
    vectors.zs = Values{7.0f, 8.0f, 9.0f};

    ml::fill(vectors.get_view().right(2), 0.0f);

    Values const expected_xs{1.0f, 0.0f, 0.0f};
    Values const expected_ys{4.0f, 0.0f, 0.0f};
    Values const expected_zs{7.0f, 0.0f, 0.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}
