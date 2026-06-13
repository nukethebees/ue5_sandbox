#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.append.Components") {
    FVectors3f vectors;

    ml::append(vectors, 1.0f, 2.0f, 3.0f);
    ml::append(vectors, 4.0f, 5.0f, 6.0f);

    auto const expected{ml::make_vectors3f({1.0f, 4.0f}, {2.0f, 5.0f}, {3.0f, 6.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.append.Vector3f") {
    FVectors3f vectors;

    ml::append(vectors, FVector3f{1.0f, 2.0f, 3.0f});

    auto const expected{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.append.Vector3d") {
    FVectors3f vectors;

    ml::append(vectors, FVector{1.0, 2.0, 3.0});

    auto const expected{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}
