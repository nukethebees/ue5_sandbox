#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.direction.Vectors3f") {
    FVectors3f out;
    out.set_num_uninitialised(3);

    auto const a{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}, {2.0f, 3.0f, 4.0f}})};
    auto const b{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 0.0f, 0.0f}, {4.0f, 6.0f, 3.0f}, {2.0f, 3.0f, 4.0f}})};

    ml::direction(out, a, b);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 0.0f, 0.0f}, {0.6f, 0.8f, 0.0f}, {0.0f, 0.0f, 0.0f}})};
    CHECK(ml::almost_equal(out, expected));
    CHECK(ml::all_normalised(out.get_const_view().left(2)));
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction.Views") {
    auto out{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}})};
    auto a{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}})};
    auto b{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 1.0f, 0.0f}, {1.0f, 2.0f, 4.0f}})};

    auto out_view{out.get_view()};
    auto a_view{a.get_view()};
    auto b_view{b.get_view()};

    ml::direction(out_view, a_view, b_view);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}})};
    CHECK(ml::almost_equal(out, expected));
    CHECK(ml::all_normalised(out.get_const_view()));
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction.OutputCanBeLargerThanInput") {
    auto out{ml::make_vectors3f(TArray<FVector3f>{{9.0f, 9.0f, 9.0f}, {9.0f, 9.0f, 9.0f}, {9.0f, 9.0f, 9.0f}})};
    auto const a{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}})};
    auto const b{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 0.0f, 0.0f}, {1.0f, 3.0f, 3.0f}})};

    ml::direction(out, a, b);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {9.0f, 9.0f, 9.0f}})};
    CHECK(ml::almost_equal(out, expected));
    CHECK(ml::all_normalised(out.get_const_view().left(2)));
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction.Empty") {
    FVectors3f out;
    FVectors3f const a;
    FVectors3f const b;

    ml::direction(out, a, b);

    CHECK(out.is_empty());
    CHECK(ml::almost_equal(out, FVectors3f{}));
    CHECK(ml::all_normalised(out.get_const_view()));
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction_and_distance.Vectors3f") {
    FVectors3f out;
    out.set_num_uninitialised(3);
    TArray<float> distances;
    distances.SetNumUninitialized(3);

    auto const from{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}})};
    auto const to{ml::make_vectors3f(TArray<FVector3f>{{2.0f, 0.0f, 0.0f}, {4.0f, 6.0f, 3.0f}, {4.0f, 5.0f, 6.0f}})};

    ml::direction_and_distance(out, distances, from, to);

    auto const expected_directions{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 0.0f, 0.0f}, {0.6f, 0.8f, 0.0f}, {0.0f, 0.0f, 0.0f}})};
    CHECK(ml::almost_equal(out, expected_directions));
    CHECK(FMath::IsNearlyEqual(distances[0], 2.0f));
    CHECK(FMath::IsNearlyEqual(distances[1], 5.0f));
    CHECK(distances[2] == 0.0f);
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction_and_distance.Views") {
    auto out{ml::make_vectors3f(TArray<FVector3f>{{9.0f, 9.0f, 9.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}})};
    auto const from{ml::make_vectors3f(TArray<FVector3f>{{9.0f, 9.0f, 9.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}})};
    auto const to{ml::make_vectors3f(TArray<FVector3f>{{9.0f, 9.0f, 9.0f}, {0.0f, 3.0f, 0.0f}, {1.0f, 1.0f, 5.0f}})};
    TArray<float> distances{-1.0f, -1.0f, -1.0f};

    ml::direction_and_distance(out.get_view().slice(1, 2),
                               TArrayView<float>{distances}.Slice(1, 2),
                               from.get_const_view().slice(1, 2),
                               to.get_const_view().slice(1, 2));

    CHECK(ml::get_vector3f(out, 0) == FVector3f(9.0f, 9.0f, 9.0f));
    CHECK(ml::get_vector3f(out, 1) == FVector3f(0.0f, 1.0f, 0.0f));
    CHECK(ml::get_vector3f(out, 2) == FVector3f(0.0f, 0.0f, 1.0f));
    CHECK(distances[0] == -1.0f);
    CHECK(distances[1] == 3.0f);
    CHECK(distances[2] == 4.0f);
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction_and_distance.Empty") {
    FVectors3f out;
    FVectors3f const from;
    FVectors3f const to;
    TArray<float> distances;

    ml::direction_and_distance(out, distances, from, to);

    CHECK(out.is_empty());
    CHECK(distances.IsEmpty());
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction_and_distance.Clamps movement without overshoot") {
    auto locations{ml::make_vectors3f(TArray<FVector3f>{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}})};
    auto const targets{ml::make_vectors3f(TArray<FVector3f>{{10.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}})};
    FVectors3f directions;
    directions.set_num_uninitialised(2);
    TArray<float> move_distances;
    move_distances.SetNumUninitialized(2);
    TArray<float> const max_move_distances{2.0f, 5.0f};

    ml::direction_and_distance(directions, move_distances, locations, targets);
    for (int32 i{0}; i < move_distances.Num(); ++i) {
        move_distances[i] = FMath::Min(move_distances[i], max_move_distances[i]);
    }
    ml::add_scaled_in_place(locations, directions, TConstArrayView<float>{move_distances}, 1.0f);

    CHECK(ml::get_vector3f(locations, 0) == FVector3f(2.0f, 0.0f, 0.0f));
    CHECK(ml::get_vector3f(locations, 1) == FVector3f(3.0f, 0.0f, 0.0f));
}
