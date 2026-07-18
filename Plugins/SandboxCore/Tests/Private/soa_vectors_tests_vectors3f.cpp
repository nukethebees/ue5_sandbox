#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectors.vectors3f.DefaultIsEmpty") {
    FVectors3f vectors;

    CHECK(vectors.num() == 0);
    CHECK(vectors.is_empty());
    CHECK(vectors.xs.IsEmpty());
    CHECK(vectors.ys.IsEmpty());
    CHECK(vectors.zs.IsEmpty());
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.GetView") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f})};

    auto view{vectors.get_view()};
    view.xs[1] = 20.0f;
    view.ys[1] = 50.0f;
    view.zs[1] = 80.0f;

    auto const expected{ml::make_vectors3f({1.0f, 20.0f, 3.0f}, {4.0f, 50.0f, 6.0f}, {7.0f, 80.0f, 9.0f})};

    CHECK(view.num() == 3);
    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.GetConstView") {
    auto const vectors{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    auto const& const_vectors{vectors};
    auto view{const_vectors.get_const_view()};

    CHECK(view.num() == 2);
    CHECK(view.xs[0] == 1.0f);
    CHECK(view.ys[1] == 4.0f);
    CHECK(view.zs[1] == 6.0f);
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.Slice") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f, 3.0f, 4.0f}, {5.0f, 6.0f, 7.0f, 8.0f}, {9.0f, 10.0f, 11.0f, 12.0f})};

    auto slice{vectors.get_view().slice(1, 2)};
    slice.xs[0] = 20.0f;
    slice.ys[1] = 70.0f;
    slice.zs[0] = 100.0f;

    auto const expected{ml::make_vectors3f({1.0f, 20.0f, 3.0f, 4.0f}, {5.0f, 6.0f, 70.0f, 8.0f}, {9.0f, 100.0f, 11.0f, 12.0f})};

    CHECK(slice.num() == 2);
    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.LeftAndRight") {
    auto const vectors{ml::make_vectors3f({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f})};

    auto view{vectors.get_const_view()};
    auto left{view.left(2)};
    auto right{view.right(2)};

    CHECK(left.num() == 2);
    CHECK(left.xs[0] == 1.0f);
    CHECK(left.ys[1] == 5.0f);
    CHECK(left.zs[1] == 8.0f);

    CHECK(right.num() == 2);
    CHECK(right.xs[0] == 2.0f);
    CHECK(right.ys[1] == 6.0f);
    CHECK(right.zs[1] == 9.0f);
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.SetNum") {
    FVectors3f vectors;

    vectors.set_num(2, EAllowShrinking::No);

    CHECK(vectors.num() == 2);
    CHECK(vectors.xs.Num() == 2);
    CHECK(vectors.ys.Num() == 2);
    CHECK(vectors.zs.Num() == 2);
    CHECK_FALSE(vectors.is_empty());
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.CopyElement") {
    auto dst{ml::make_vectors3f({1.0f, 2.0f, 3.0f},
                                {4.0f, 5.0f, 6.0f},
                                {7.0f, 8.0f, 9.0f})};
    auto const src{ml::make_vectors3f({10.0f, 20.0f, 30.0f},
                                      {40.0f, 50.0f, 60.0f},
                                      {70.0f, 80.0f, 90.0f})};

    dst.copy_element(1, src, 2);

    auto const expected{ml::make_vectors3f({1.0f, 30.0f, 3.0f},
                                            {4.0f, 60.0f, 6.0f},
                                            {7.0f, 90.0f, 9.0f})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.Reserve") {
    FVectors3f vectors;

    vectors.reserve(3);

    CHECK(vectors.num() == 0);
    CHECK(vectors.xs.Max() >= 3);
    CHECK(vectors.ys.Max() >= 3);
    CHECK(vectors.zs.Max() >= 3);
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.AddUninitialized") {
    FVectors3f vectors;

    ml::add_uninitialised(vectors, 3);

    CHECK(vectors.num() == 3);
    CHECK(vectors.xs.Num() == 3);
    CHECK(vectors.ys.Num() == 3);
    CHECK(vectors.zs.Num() == 3);
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.Reset") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    vectors.reset();

    CHECK(vectors.num() == 0);
    CHECK(vectors.is_empty());
    CHECK(vectors.xs.IsEmpty());
    CHECK(vectors.ys.IsEmpty());
    CHECK(vectors.zs.IsEmpty());
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.RemoveAtSwap") {
    auto vectors{ml::make_vectors3f({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f})};

    vectors.remove_at_swap(1, 1, EAllowShrinking::No);

    auto const expected{ml::make_vectors3f({1.0f, 3.0f}, {4.0f, 6.0f}, {7.0f, 9.0f})};

    CHECK(vectors.num() == 2);
    CHECK(ml::almost_equal(vectors, expected));
}
