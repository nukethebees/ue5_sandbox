#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
template <typename T>
struct TTestContiguousContainer {
    T const* data{nullptr};
    int32 count{0};

    auto Num() const -> int32 { return count; }
    auto GetData() const -> T const* { return data; }
};

static_assert(ml::HasNumAndGetData<TArray<int32>>);
static_assert(ml::HasNumAndGetData<TArrayView<int32>>);
static_assert(ml::HasNumAndGetData<TConstArrayView<int32>>);
static_assert(ml::HasNumAndGetData<TTestContiguousContainer<int32>>);
static_assert(!ml::HasNumAndGetData<int32>);
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Distinct arrays") {
    TArray<int32> const first{1, 2};
    TArray<int32> const second{3, 4};
    TArray<int32> const third{5, 6};

    CHECK(ml::all_num_equal_and_pointers_not_equal(first, second, third));
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Mutable and const views") {
    TArray<int32> first_values{1, 2};
    TArray<int32> const second_values{3, 4};
    TArrayView<int32> const first{first_values};
    TConstArrayView<int32> const second{second_values};

    CHECK(ml::all_num_equal_and_pointers_not_equal(first, second));
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Heterogeneous types") {
    TArray<int32> const integers{1, 2};
    TArray<float> const floats{1.0f, 2.0f};

    CHECK(ml::all_num_equal_and_pointers_not_equal(integers, floats));
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Custom containers") {
    TArray<int32> const first_values{1, 2};
    TArray<int32> const second_values{3, 4};
    TTestContiguousContainer<int32> const first{first_values.GetData(), first_values.Num()};
    TTestContiguousContainer<int32> const second{second_values.GetData(), second_values.Num()};

    CHECK(ml::all_num_equal_and_pointers_not_equal(first, second));
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Different lengths") {
    TArray<int32> const first{1};
    TArray<int32> const second{2, 3};

    CHECK_FALSE(ml::all_num_equal_and_pointers_not_equal(first, second));
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Aliased array") {
    TArray<int32> const values{1, 2};

    CHECK_FALSE(ml::all_num_equal_and_pointers_not_equal(values, values));
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Aliased variadic input") {
    TArray<int32> const first{1, 2};
    TArray<int32> const second{3, 4};
    TConstArrayView<int32> const second_view{second};

    CHECK_FALSE(ml::all_num_equal_and_pointers_not_equal(first, second, second_view));
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Empty containers") {
    TArray<int32> const first;
    TArray<int32> const second;

    CHECK_FALSE(ml::all_num_equal_and_pointers_not_equal(first, second));
}

TEST_CASE("SandboxCore.Array.all_num_equal_and_pointers_not_equal.Overlapping offset views") {
    TArray<int32> const values{1, 2, 3};
    auto const left{TConstArrayView<int32>{values}.Left(2)};
    auto const right{TConstArrayView<int32>{values}.Right(2)};

    CHECK(ml::all_num_equal_and_pointers_not_equal(left, right));
}
