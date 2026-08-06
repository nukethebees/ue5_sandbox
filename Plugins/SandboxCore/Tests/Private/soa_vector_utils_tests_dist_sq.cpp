#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.dist_sq.Vector3f") {
    auto const vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}, {2.0f, 3.0f, 4.0f}})};

    auto const result{ml::dist_sq(vectors, 0, FVector3f{1.0f, 3.0f, 5.0f})};

    CHECK(result == 5.0f);
}

TEST_CASE("SandboxCore.SoaVectorUtils.dist_sq.Components") {
    auto const vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}, {2.0f, 3.0f, 4.0f}})};

    auto const result{ml::dist_sq(vectors, 1, 1.0f, 1.0f, 1.0f)};

    CHECK(result == 14.0f);
}

TEST_CASE("SandboxCore.SoaVectorUtils.dist_and_dist_sq.Vectors3f") {
    auto const from{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}})};
    auto const to{ml::make_vectors3f(TArray<FVector3f>{{3.0f, 4.0f, 0.0f}, {3.0f, 5.0f, 9.0f}, {4.0f, 5.0f, 6.0f}})};
    TArray<float> distances;
    distances.SetNumUninitialized(3);
    TArray<float> distances_sq;
    distances_sq.SetNumUninitialized(3);
    TArray<float> const expected_distances{5.0f, 7.0f, 0.0f};
    TArray<float> const expected_distances_sq{25.0f, 49.0f, 0.0f};

    ml::dist_and_dist_sq(distances, distances_sq, from, to);

    CHECK(distances == expected_distances);
    CHECK(distances_sq == expected_distances_sq);
}

TEST_CASE("SandboxCore.SoaVectorUtils.dist_and_dist_sq.Views") {
    auto const from{ml::make_vectors3f(TArray<FVector3f>{{9.0f, 9.0f, 9.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}})};
    auto const to{ml::make_vectors3f(TArray<FVector3f>{{9.0f, 9.0f, 9.0f}, {0.0f, 3.0f, 4.0f}, {3.0f, 4.0f, 7.0f}})};
    TArray<float> distances{-1.0f, -1.0f, -1.0f};
    TArray<float> distances_sq{-1.0f, -1.0f, -1.0f};
    TArray<float> const expected_distances{-1.0f, 5.0f, 7.0f};
    TArray<float> const expected_distances_sq{-1.0f, 25.0f, 49.0f};

    ml::dist_and_dist_sq(TArrayView<float>{distances}.Slice(1, 2),
                         TArrayView<float>{distances_sq}.Slice(1, 2),
                         from.get_const_view().slice(1, 2),
                         to.get_const_view().slice(1, 2));

    CHECK(distances == expected_distances);
    CHECK(distances_sq == expected_distances_sq);
}

TEST_CASE("SandboxCore.SoaVectorUtils.dist_and_dist_sq.Empty") {
    FVectors3f const from;
    FVectors3f const to;
    TArray<float> distances;
    TArray<float> distances_sq;

    ml::dist_and_dist_sq(distances, distances_sq, from, to);

    CHECK(distances.IsEmpty());
    CHECK(distances_sq.IsEmpty());
}
