#include <SandboxCore/array_math.h>

#include <Containers/StaticArray.h>
#include "TestHarness.h"

TEST_CASE("SandboxCore.ArrayMath.CollectIndicesOwningOutput") {
    TArray<int32> output{99, 98};
    TArray<int32> const values{4, -2, 0, 7, -1};

    SECTION("Sparse matches include the threshold and retain input order") {
        auto const matches{ml::collect_indices_less_equal(TConstArrayView<int32>{values}, 0, output)};
        CHECK((output == TArray<int32>{1, 2, 4}));
        CHECK(matches.Num() == output.Num());
        CHECK(matches.GetData() == output.GetData());
        CHECK((values == TArray<int32>{4, -2, 0, 7, -1}));
    }
    SECTION("All matches") {
        ml::collect_indices_less_equal(TConstArrayView<int32>{values}, 7, output);
        CHECK((output == TArray<int32>{0, 1, 2, 3, 4}));
    }
    SECTION("No matches discard previous output") {
        auto const matches{ml::collect_indices_less_equal(TConstArrayView<int32>{values}, -3, output)};
        CHECK(output.IsEmpty());
        CHECK(matches.IsEmpty());
    }
    SECTION("Empty input discards previous output") {
        auto const matches{ml::collect_indices_less_equal(TConstArrayView<int32>{}, 0, output)};
        CHECK(output.IsEmpty());
        CHECK(matches.IsEmpty());
    }
    SECTION("Repeated calls retain capacity and only expose current matches") {
        output.Reserve(16);
        auto const capacity{output.Max()};
        auto const* storage{output.GetData()};
        ml::collect_indices_less_equal(TConstArrayView<int32>{values}, 7, output);
        ml::collect_indices_less_equal(TConstArrayView<int32>{values}, -2, output);
        CHECK((output == TArray<int32>{1}));
        ml::collect_indices_less_equal(TConstArrayView<int32>{}, 0, output);
        CHECK(output.IsEmpty());
        CHECK(output.Max() == capacity);
        CHECK(output.GetData() == storage);
    }
    SECTION("No matches do not allocate worst-case storage") {
        TArray<int32> fresh_output;
        ml::collect_indices_less_equal(TConstArrayView<int32>{values}, -3, fresh_output);
        CHECK(fresh_output.Max() == 0);
    }
}

TEST_CASE("SandboxCore.ArrayMath.CollectIndicesNumericViews") {
    TArray<int32> output;
    SECTION("Unsigned alive flags select exactly zero") {
        TArray<uint8> const alive{1, 0, 2, 255, 0};
        ml::collect_indices_less_equal(TConstArrayView<uint8>{alive}, uint8{0}, output);
        CHECK((output == TArray<int32>{1, 4}));
    }
    SECTION("Float subviews use relative indices and agree with the view overload") {
        TArray<float> const values{-9.f, 0.5f, 0.f, -0.5f, 1.f};
        auto const slice{TConstArrayView<float>{values}.Slice(1, 3)};
        ml::collect_indices_less_equal(slice, 0.f, output);
        CHECK((output == TArray<int32>{1, 2}));
        TArray<int32> buffer;
        buffer.SetNumUninitialized(slice.Num());
        auto const matches{ml::collect_indices_less_equal(slice, 0.f, TArrayView<int32>{buffer})};
        REQUIRE(matches.Num() == output.Num());
        auto const count{matches.Num()};
        for (int32 index{}; index < count; ++index) {
            CHECK(matches[index] == output[index]);
        }
    }
}

TEST_CASE("SandboxCore.ArrayMath.SumViews") {
    CHECK(ml::sum(TConstArrayView<int32>{}) == 0);
    TArray<int32> const values{10, -3, 7, 99};
    CHECK(ml::sum(TConstArrayView<int32>{values}.Slice(1, 2)) == 4);
    TStaticArray<int32, 5> const counts{0, 2, 3, 0, 4};
    CHECK(ml::sum(TConstArrayView<int32>{counts}) == 9);
}
