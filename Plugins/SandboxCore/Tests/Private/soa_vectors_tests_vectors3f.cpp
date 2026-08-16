#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <type_traits>
#include <utility>

static_assert(std::is_same_v<FVectors3f::aos_type, FVector3f>);
static_assert(std::is_aggregate_v<FVectors3f::Data>);
static_assert(std::is_aggregate_v<FVectors3f::ConstData>);
static_assert(std::is_same_v<decltype(FVectors3f::Data::xs), float*>);
static_assert(std::is_same_v<decltype(FVectors3f::Data::ys), float*>);
static_assert(std::is_same_v<decltype(FVectors3f::Data::zs), float*>);
static_assert(std::is_same_v<decltype(FVectors3f::ConstData::xs), float const*>);
static_assert(std::is_same_v<decltype(FVectors3f::ConstData::ys), float const*>);
static_assert(std::is_same_v<decltype(FVectors3f::ConstData::zs), float const*>);
static_assert(std::is_same_v<decltype(std::declval<FVectors3f&>().get_data()), FVectors3f::Data>);
static_assert(std::is_same_v<decltype(std::declval<FVectors3f const&>().get_data()), FVectors3f::ConstData>);

TEST_CASE("SandboxCore.SoaVectors.vectors3f.DefaultIsEmpty") {
    FVectors3f vectors;

    CHECK(vectors.num() == 0);
    CHECK(vectors.is_empty());
    CHECK(vectors.xs.IsEmpty());
    CHECK(vectors.ys.IsEmpty());
    CHECK(vectors.zs.IsEmpty());
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.AddComponents") {
    FVectors3f vectors;

    auto const first_index{vectors.add(1.0f, 2.0f, 3.0f)};
    auto const second_index{vectors.add(4.0f, 5.0f, 6.0f)};

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}})};
    CHECK(first_index == 0);
    CHECK(second_index == 1);
    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.AddAosValue") {
    FVectors3f vectors;

    auto const first_index{vectors.add(FVector3f{1.0f, 2.0f, 3.0f})};
    auto const second_index{vectors.add(FVector3f{4.0f, 5.0f, 6.0f})};

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}})};
    CHECK(first_index == 0);
    CHECK(second_index == 1);
    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.GetData") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}})};

    auto [xs, ys, zs] = vectors.get_data();

    CHECK(xs == vectors.xs.GetData());
    CHECK(ys == vectors.ys.GetData());
    CHECK(zs == vectors.zs.GetData());
    xs[1] = 40.0f;
    CHECK(vectors.xs[1] == 40.0f);
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.GetDataConst") {
    auto const vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}})};

    auto [xs, ys, zs] = vectors.get_data();

    static_assert(std::is_same_v<decltype(xs), float const*>);
    static_assert(std::is_same_v<decltype(ys), float const*>);
    static_assert(std::is_same_v<decltype(zs), float const*>);
    CHECK(xs == vectors.xs.GetData());
    CHECK(ys == vectors.ys.GetData());
    CHECK(zs == vectors.zs.GetData());
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.GetView") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}})};

    auto view{vectors.get_view()};
    view.xs[1] = 20.0f;
    view.ys[1] = 50.0f;
    view.zs[1] = 80.0f;

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {20.0f, 50.0f, 80.0f}, {3.0f, 6.0f, 9.0f}})};

    CHECK(view.num() == 3);
    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.GetConstView") {
    auto const vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, 5.0f}, {2.0f, 4.0f, 6.0f}})};

    auto const& const_vectors{vectors};
    auto view{const_vectors.get_const_view()};

    CHECK(view.num() == 2);
    CHECK(view.xs[0] == 1.0f);
    CHECK(view.ys[1] == 4.0f);
    CHECK(view.zs[1] == 6.0f);
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.GetViewWithOffsetAndCount") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 5.0f, 9.0f}, {2.0f, 6.0f, 10.0f}, {3.0f, 7.0f, 11.0f}, {4.0f, 8.0f, 12.0f}})};

    auto view{vectors.get_view(1, 2)};
    view.xs[0] = 20.0f;
    view.ys[1] = 70.0f;
    view.zs[0] = 100.0f;

    auto const expected{
        ml::make_vectors3f(TArray<FVector3f>{{1.0f, 5.0f, 9.0f}, {20.0f, 6.0f, 100.0f}, {3.0f, 70.0f, 11.0f}, {4.0f, 8.0f, 12.0f}})};

    CHECK(view.num() == 2);
    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.GetConstViewsWithOffsetAndCount") {
    auto const vectors{
        ml::make_vectors3f(TArray<FVector3f>{{1.0f, 5.0f, 9.0f}, {2.0f, 6.0f, 10.0f}, {3.0f, 7.0f, 11.0f}, {4.0f, 8.0f, 12.0f}})};

    auto view{vectors.get_view(1, 2)};
    auto const_view{vectors.get_const_view(1, 2)};

    CHECK(view.num() == 2);
    CHECK(view.xs[0] == 2.0f);
    CHECK(view.ys[1] == 7.0f);
    CHECK(view.zs[1] == 11.0f);
    CHECK(const_view.num() == 2);
    CHECK(const_view.xs[0] == 2.0f);
    CHECK(const_view.ys[1] == 7.0f);
    CHECK(const_view.zs[1] == 11.0f);
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.Slice") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 5.0f, 9.0f}, {2.0f, 6.0f, 10.0f}, {3.0f, 7.0f, 11.0f}, {4.0f, 8.0f, 12.0f}})};

    auto slice{vectors.get_view().slice(1, 2)};
    slice.xs[0] = 20.0f;
    slice.ys[1] = 70.0f;
    slice.zs[0] = 100.0f;

    auto const expected{
        ml::make_vectors3f(TArray<FVector3f>{{1.0f, 5.0f, 9.0f}, {20.0f, 6.0f, 100.0f}, {3.0f, 70.0f, 11.0f}, {4.0f, 8.0f, 12.0f}})};

    CHECK(slice.num() == 2);
    CHECK(ml::almost_equal(vectors, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.LeftAndRight") {
    auto const vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}})};

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
    auto dst{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}})};
    auto const src{ml::make_vectors3f(TArray<FVector3f>{{10.0f, 40.0f, 70.0f}, {20.0f, 50.0f, 80.0f}, {30.0f, 60.0f, 90.0f}})};

    dst.copy_element(1, src, 2);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {30.0f, 60.0f, 90.0f}, {3.0f, 6.0f, 9.0f}})};

    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.CopyElements") {
    auto dst{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}})};
    auto const src{ml::make_vectors3f(TArray<FVector3f>{{10.0f, 40.0f, 70.0f}, {20.0f, 50.0f, 80.0f}, {30.0f, 60.0f, 90.0f}})};

    dst.copy_elements(0, src, 0, src.num());
    CHECK(ml::almost_equal(dst, src));

    dst.copy_elements(1, src, 2, 1);
    CHECK(dst.xs[1] == 30.0f);
    CHECK(dst.ys[1] == 60.0f);
    CHECK(dst.zs[1] == 90.0f);

    auto const expected{dst};
    dst.copy_elements(dst.num(), src, src.num(), 0);
    CHECK(ml::almost_equal(dst, expected));
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.CopyToTail") {
    auto dst{ml::make_vectors3f(TArray<FVector3f>{
        {1.0f, 11.0f, 21.0f}, {2.0f, 12.0f, 22.0f}, {3.0f, 13.0f, 23.0f},
        {4.0f, 14.0f, 24.0f}, {5.0f, 15.0f, 25.0f}})};
    auto const src{ml::make_vectors3f(
        TArray<FVector3f>{{60.0f, 70.0f, 80.0f}, {90.0f, 100.0f, 110.0f}})};

    dst.copy_to_tail(src);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{
        {1.0f, 11.0f, 21.0f}, {2.0f, 12.0f, 22.0f}, {3.0f, 13.0f, 23.0f},
        {60.0f, 70.0f, 80.0f}, {90.0f, 100.0f, 110.0f}})};
    CHECK(ml::almost_equal(dst, expected));

    FVectors3f const empty_src;
    dst.copy_to_tail(empty_src);
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
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 3.0f, 5.0f}, {2.0f, 4.0f, 6.0f}})};

    vectors.reset();

    CHECK(vectors.num() == 0);
    CHECK(vectors.is_empty());
    CHECK(vectors.xs.IsEmpty());
    CHECK(vectors.ys.IsEmpty());
    CHECK(vectors.zs.IsEmpty());
}

TEST_CASE("SandboxCore.SoaVectors.vectors3f.RemoveAtSwap") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f}})};

    vectors.remove_at_swap(1, 1, EAllowShrinking::No);

    auto const expected{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 4.0f, 7.0f}, {3.0f, 6.0f, 9.0f}})};

    CHECK(vectors.num() == 2);
    CHECK(ml::almost_equal(vectors, expected));
}
