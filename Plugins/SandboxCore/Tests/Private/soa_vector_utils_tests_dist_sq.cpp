#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.dist_sq.Vector3f") {
    auto const vectors{ml::make_vectors3f({1.0f, 2.0f}, {2.0f, 3.0f}, {3.0f, 4.0f})};

    auto const result{ml::dist_sq(vectors, 0, FVector3f{1.0f, 3.0f, 5.0f})};

    CHECK(result == 5.0f);
}

TEST_CASE("SandboxCore.SoaVectorUtils.dist_sq.Components") {
    auto const vectors{ml::make_vectors3f({1.0f, 2.0f}, {2.0f, 3.0f}, {3.0f, 4.0f})};

    auto const result{ml::dist_sq(vectors, 1, 1.0f, 1.0f, 1.0f)};

    CHECK(result == 14.0f);
}
