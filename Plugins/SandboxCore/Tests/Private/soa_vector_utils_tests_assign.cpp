#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.assign.Value") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    ml::assign(vectors, 0, 4.0f);

    auto const expected{ml::make_vectors3f({4.0f}, {4.0f}, {4.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign.Components") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    ml::assign(vectors, 0, 4.0f, 5.0f, 6.0f);

    auto const expected{ml::make_vectors3f({4.0f}, {5.0f}, {6.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign.Vector3f") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    ml::assign(vectors, 0, FVector3f{4.0f, 5.0f, 6.0f});

    auto const expected{ml::make_vectors3f({4.0f}, {5.0f}, {6.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign.Vector3d") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    ml::assign(vectors, 0, FVector{4.0, 5.0, 6.0});

    auto const expected{ml::make_vectors3f({4.0f}, {5.0f}, {6.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}
