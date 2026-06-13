#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.get_vector3f.Vectors") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 4.0f};
    vectors.ys = Values{2.0f, 5.0f};
    vectors.zs = Values{3.0f, 6.0f};

    auto const result{ml::get_vector3f(vectors, 1)};

    CHECK(result.X == 4.0f);
    CHECK(result.Y == 5.0f);
    CHECK(result.Z == 6.0f);
}

TEST_CASE("SandboxCore.SoaVectorUtils.get_vector3d.Vectors") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 4.0f};
    vectors.ys = Values{2.0f, 5.0f};
    vectors.zs = Values{3.0f, 6.0f};

    auto const result{ml::get_vector3d(vectors, 1)};

    CHECK(result.X == 4.0);
    CHECK(result.Y == 5.0);
    CHECK(result.Z == 6.0);
}

TEST_CASE("SandboxCore.SoaVectorUtils.get_vector3d.View") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 4.0f};
    vectors.ys = Values{2.0f, 5.0f};
    vectors.zs = Values{3.0f, 6.0f};

    auto const result{ml::get_vector3d(vectors.get_const_view().right(1), 0)};

    CHECK(result.X == 4.0);
    CHECK(result.Y == 5.0);
    CHECK(result.Z == 6.0);
}

TEST_CASE("SandboxCore.SoaVectorUtils.scaled_vector3d.Vectors") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f, 4.0f};
    vectors.ys = Values{2.0f, 5.0f};
    vectors.zs = Values{3.0f, 6.0f};

    auto const result{ml::scaled_vector3d(vectors, 0, 2.0f)};

    CHECK(result.X == 2.0);
    CHECK(result.Y == 4.0);
    CHECK(result.Z == 6.0);
}
