#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.all_normalised.Empty") {
    FVectors3f const vectors;

    CHECK(ml::all_normalised(vectors.get_const_view()));
}

TEST_CASE("SandboxCore.SoaVectorUtils.all_normalised.UnitAxes") {
    auto const vectors{ml::make_vectors3f({1.0f, 0.0f, 0.0f},
                                          {0.0f, 1.0f, 0.0f},
                                          {0.0f, 0.0f, 1.0f})};

    CHECK(ml::all_normalised(vectors.get_const_view()));
}

TEST_CASE("SandboxCore.SoaVectorUtils.all_normalised.UnitDiagonal") {
    auto const vectors{ml::make_vectors3f({0.6f}, {0.8f}, {0.0f})};

    CHECK(ml::all_normalised(vectors.get_const_view()));
}

TEST_CASE("SandboxCore.SoaVectorUtils.all_normalised.ZeroIsNotNormalised") {
    auto const vectors{ml::make_vectors3f({0.0f}, {0.0f}, {0.0f})};

    CHECK(!ml::all_normalised(vectors.get_const_view()));
}

TEST_CASE("SandboxCore.SoaVectorUtils.all_normalised.LargerVectorIsNotNormalised") {
    auto const vectors{ml::make_vectors3f({2.0f}, {0.0f}, {0.0f})};

    CHECK(!ml::all_normalised(vectors.get_const_view()));
}
