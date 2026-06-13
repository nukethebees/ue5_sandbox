#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.fill.Vectors") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    ml::fill(vectors, 7.0f);

    auto const expected{ml::make_vectors3f({7.0f, 7.0f}, {7.0f, 7.0f}, {7.0f, 7.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.fill.View") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f})};

    ml::fill(vectors.get_view().right(2), 0.0f);

    auto const expected{ml::make_vectors3f({1.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {7.0f, 0.0f, 0.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}
