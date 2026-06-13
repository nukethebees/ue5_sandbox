#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.add_scaled_in_place.Zero") {
    auto dst{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};
    auto const src{ml::make_vectors3f({1.0f, 1.0f}, {2.0f, 2.0f}, {3.0f, 3.0f})};

    ml::add_scaled_in_place(dst, src, 0.0f);

    auto const expected{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.add_scaled_in_place.Two") {
    auto dst{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};
    auto const src{ml::make_vectors3f({1.0f, 1.0f}, {2.0f, 2.0f}, {3.0f, 3.0f})};

    ml::add_scaled_in_place(dst, src, 2.0f);

    auto const expected{ml::make_vectors3f({3.0f, 4.0f}, {7.0f, 8.0f}, {11.0f, 12.0f})};

    CHECK(ml::almost_equal(dst, expected));
}
