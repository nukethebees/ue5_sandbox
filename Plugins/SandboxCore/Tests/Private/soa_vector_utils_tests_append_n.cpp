#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.append_n.Components") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    ml::append_n(vectors, 2, 4.0f, 5.0f, 6.0f);

    auto const expected{ml::make_vectors3f({1.0f, 4.0f, 4.0f}, {2.0f, 5.0f, 5.0f}, {3.0f, 6.0f, 6.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.append_n.Value") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    ml::append_n(vectors, 2, 4.0f);

    auto const expected{ml::make_vectors3f({1.0f, 4.0f, 4.0f}, {2.0f, 4.0f, 4.0f}, {3.0f, 4.0f, 4.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.add_zeroed.Two") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};

    ml::add_zeroed(vectors, 2);

    auto const expected{ml::make_vectors3f({1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}
