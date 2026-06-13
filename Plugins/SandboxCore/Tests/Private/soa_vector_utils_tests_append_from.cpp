#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.append_from.Vectors") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};
    auto const to_append{ml::make_vectors3f({4.0f, 7.0f}, {5.0f, 8.0f}, {6.0f, 9.0f})};

    ml::append_from(vectors, to_append);

    auto const expected{ml::make_vectors3f({1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.append_from.View") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};
    auto const to_append{ml::make_vectors3f({4.0f, 7.0f}, {5.0f, 8.0f}, {6.0f, 9.0f})};

    ml::append_from(vectors, to_append.get_const_view().right(1));

    auto const expected{ml::make_vectors3f({1.0f, 7.0f}, {2.0f, 8.0f}, {3.0f, 9.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.append_element_from.Vectors") {
    auto vectors{ml::make_vectors3f({1.0f}, {2.0f}, {3.0f})};
    auto const to_append{ml::make_vectors3f({4.0f, 7.0f}, {5.0f, 8.0f}, {6.0f, 9.0f})};

    ml::append_element_from(vectors, to_append, 1);

    auto const expected{ml::make_vectors3f({1.0f, 7.0f}, {2.0f, 8.0f}, {3.0f, 9.0f})};

    CHECK(ml::almost_equal(vectors, expected));
}
