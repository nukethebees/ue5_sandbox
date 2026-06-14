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

TEST_CASE("SandboxCore.SoaVectorUtils.add_scaled_in_place.ComponentWiseScale") {
    auto dst{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};
    auto const a{ml::make_vectors3f({10.0f, 20.0f}, {30.0f, 40.0f}, {50.0f, 60.0f})};
    auto const b{ml::make_vectors3f({2.0f, 3.0f}, {4.0f, 5.0f}, {6.0f, 7.0f})};

    ml::add_scaled_in_place(dst, a, b, 0.5f);

    auto const expected{ml::make_vectors3f({11.0f, 32.0f}, {63.0f, 104.0f}, {155.0f, 216.0f})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.add_scaled_in_place.SharedScaleArray") {
    auto dst{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};
    auto const a{ml::make_vectors3f({10.0f, 20.0f}, {30.0f, 40.0f}, {50.0f, 60.0f})};
    TArray<float> const b{2.0f, 3.0f};

    ml::add_scaled_in_place(dst, a, TConstArrayView<float>{b}, 0.5f);

    auto const expected{ml::make_vectors3f({11.0f, 32.0f}, {33.0f, 64.0f}, {55.0f, 96.0f})};

    CHECK(ml::almost_equal(dst, expected));
}
