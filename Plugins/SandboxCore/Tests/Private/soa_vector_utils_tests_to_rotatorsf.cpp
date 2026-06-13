#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.to_rotatorsf.Empty") {
    FVectors3f const vectors;

    auto const result{ml::to_rotatorsf(vectors)};
    FRotatorsf const expected;

    CHECK(result.is_empty());
    CHECK(ml::almost_equal(result, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.to_rotatorsf.OutParamEmpty") {
    FVectors3f const vectors;
    auto result{ml::make_rotatorsf({1.0f}, {2.0f}, {3.0f})};

    ml::to_rotatorsf(result, vectors);
    FRotatorsf const expected;

    CHECK(result.is_empty());
    CHECK(ml::almost_equal(result, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.to_rotatorsf.Vectors") {
    auto const vectors{ml::make_vectors3f({1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f},
                                          {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f},
                                          {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f})};

    auto const result{ml::to_rotatorsf(vectors)};
    auto const expected{ml::make_rotatorsf({0.0f, 0.0f, 90.0f, 0.0f, 0.0f, 0.0f, 45.0f},
                                           {0.0f, 90.0f, 0.0f, 45.0f, 180.0f, -90.0f, 0.0f},
                                           {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f})};

    CHECK(ml::almost_equal(result, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.to_rotatorsf.OutParamVectors") {
    auto const vectors{ml::make_vectors3f({1.0f, 0.0f, 0.0f},
                                          {0.0f, 1.0f, -1.0f},
                                          {0.0f, 0.0f, 0.0f})};
    auto result{ml::make_rotatorsf({9.0f}, {9.0f}, {9.0f})};

    ml::to_rotatorsf(result, vectors);
    auto const expected{ml::make_rotatorsf({0.0f, 0.0f, 0.0f},
                                           {0.0f, 90.0f, -90.0f},
                                           {0.0f, 0.0f, 0.0f})};

    CHECK(result.num() == 3);
    CHECK(ml::almost_equal(result, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.to_rotatorsf.View") {
    auto const vectors{ml::make_vectors3f({9.0f, 1.0f, 0.0f},
                                          {9.0f, 0.0f, 1.0f},
                                          {9.0f, 0.0f, 0.0f})};

    auto const result{ml::to_rotatorsf(vectors.get_const_view().right(2))};
    auto const expected{ml::make_rotatorsf({0.0f, 0.0f}, {0.0f, 90.0f}, {0.0f, 0.0f})};

    CHECK(ml::almost_equal(result, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.to_rotatorsf.OutParamView") {
    auto const vectors{ml::make_vectors3f({9.0f, 1.0f, 0.0f},
                                          {9.0f, 0.0f, 1.0f},
                                          {9.0f, 0.0f, 0.0f})};
    FRotatorsf result;

    ml::to_rotatorsf(result, vectors.get_const_view().right(2));
    auto const expected{ml::make_rotatorsf({0.0f, 0.0f}, {0.0f, 90.0f}, {0.0f, 0.0f})};

    CHECK(ml::almost_equal(result, expected));
}
