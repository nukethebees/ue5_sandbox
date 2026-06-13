#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.direction.Vectors3f") {
    FVectors3f out;
    out.set_num_uninitialized(3);

    auto const a{
        ml::make_vectors3f({0.0f, 1.0f, 2.0f}, {0.0f, 2.0f, 3.0f}, {0.0f, 3.0f, 4.0f})};
    auto const b{
        ml::make_vectors3f({1.0f, 4.0f, 2.0f}, {0.0f, 6.0f, 3.0f}, {0.0f, 3.0f, 4.0f})};

    ml::direction(out, a, b);

    auto const expected{
        ml::make_vectors3f({1.0f, 0.6f, 0.0f}, {0.0f, 0.8f, 0.0f}, {0.0f, 0.0f, 0.0f})};
    CHECK(ml::almost_equal(out, expected));
    CHECK(ml::all_normalised(out.get_const_view().left(2)));
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction.Views") {
    auto out{ml::make_vectors3f({0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f})};
    auto a{ml::make_vectors3f({0.0f, 1.0f}, {0.0f, 2.0f}, {0.0f, 3.0f})};
    auto b{ml::make_vectors3f({0.0f, 1.0f}, {1.0f, 2.0f}, {0.0f, 4.0f})};

    auto out_view{out.get_view()};
    auto a_view{a.get_view()};
    auto b_view{b.get_view()};

    ml::direction(out_view, a_view, b_view);

    auto const expected{ml::make_vectors3f({0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f})};
    CHECK(ml::almost_equal(out, expected));
    CHECK(ml::all_normalised(out.get_const_view()));
}

TEST_CASE("SandboxCore.SoaVectorUtils.direction.OutputCanBeLargerThanInput") {
    auto out{
        ml::make_vectors3f({9.0f, 9.0f, 9.0f}, {9.0f, 9.0f, 9.0f}, {9.0f, 9.0f, 9.0f})};
    auto const a{ml::make_vectors3f({0.0f, 1.0f}, {0.0f, 2.0f}, {0.0f, 3.0f})};
    auto const b{ml::make_vectors3f({1.0f, 1.0f}, {0.0f, 3.0f}, {0.0f, 3.0f})};

    ml::direction(out, a, b);

    auto const expected{
        ml::make_vectors3f({1.0f, 0.0f, 9.0f}, {0.0f, 1.0f, 9.0f}, {0.0f, 0.0f, 9.0f})};
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
