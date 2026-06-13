#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.append.Components") {
    FVectors3f vectors;

    ml::append(vectors, 1.0f, 2.0f, 3.0f);
    ml::append(vectors, 4.0f, 5.0f, 6.0f);

    Values const expected_xs{1.0f, 4.0f};
    Values const expected_ys{2.0f, 5.0f};
    Values const expected_zs{3.0f, 6.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.append.Vector3f") {
    FVectors3f vectors;

    ml::append(vectors, FVector3f{1.0f, 2.0f, 3.0f});

    Values const expected{1.0f};
    Values const expected_ys{2.0f};
    Values const expected_zs{3.0f};

    CHECK(vectors.xs == expected);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.append.Vector3d") {
    FVectors3f vectors;

    ml::append(vectors, FVector{1.0, 2.0, 3.0});

    Values const expected_xs{1.0f};
    Values const expected_ys{2.0f};
    Values const expected_zs{3.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}
