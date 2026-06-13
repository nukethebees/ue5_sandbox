#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.append_n.Components") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    ml::append_n(vectors, 2, 4.0f, 5.0f, 6.0f);

    Values const expected_xs{1.0f, 4.0f, 4.0f};
    Values const expected_ys{2.0f, 5.0f, 5.0f};
    Values const expected_zs{3.0f, 6.0f, 6.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.append_n.Value") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    ml::append_n(vectors, 2, 4.0f);

    Values const expected_xs{1.0f, 4.0f, 4.0f};
    Values const expected_ys{2.0f, 4.0f, 4.0f};
    Values const expected_zs{3.0f, 4.0f, 4.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.add_zeroed.Two") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    ml::add_zeroed(vectors, 2);

    Values const expected_xs{1.0f, 0.0f, 0.0f};
    Values const expected_ys{2.0f, 0.0f, 0.0f};
    Values const expected_zs{3.0f, 0.0f, 0.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}
