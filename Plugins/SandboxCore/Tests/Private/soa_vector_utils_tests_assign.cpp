#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.assign.Value") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    ml::assign(vectors, 0, 4.0f);

    Values const expected{4.0f};

    CHECK(vectors.xs == expected);
    CHECK(vectors.ys == expected);
    CHECK(vectors.zs == expected);
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign.Components") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    ml::assign(vectors, 0, 4.0f, 5.0f, 6.0f);

    Values const expected_xs{4.0f};
    Values const expected_ys{5.0f};
    Values const expected_zs{6.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign.Vector3f") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    ml::assign(vectors, 0, FVector3f{4.0f, 5.0f, 6.0f});

    Values const expected_xs{4.0f};
    Values const expected_ys{5.0f};
    Values const expected_zs{6.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign.Vector3d") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    ml::assign(vectors, 0, FVector{4.0, 5.0, 6.0});

    Values const expected_xs{4.0f};
    Values const expected_ys{5.0f};
    Values const expected_zs{6.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}
