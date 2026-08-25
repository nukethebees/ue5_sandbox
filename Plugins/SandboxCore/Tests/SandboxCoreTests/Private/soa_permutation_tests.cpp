#include <SandboxCore/soa_permutation.h>

#include "TestHarness.h"

namespace {
struct FMoveOnlyValue {
    explicit FMoveOnlyValue(int32 const value)
        : value{value} {}

    FMoveOnlyValue(FMoveOnlyValue&&) = default;
    auto operator=(FMoveOnlyValue&&) -> FMoveOnlyValue& = default;

    FMoveOnlyValue(FMoveOnlyValue const&) = delete;
    auto operator=(FMoveOnlyValue const&) -> FMoveOnlyValue& = delete;

    int32 value{};
};
}

TEST_CASE("SandboxCore.SoaPermutation.MoveOnlyValuesAndDisjointCycles") {
    TArray<FMoveOnlyValue> values;
    values.Emplace(10);
    values.Emplace(20);
    values.Emplace(30);
    values.Emplace(40);
    values.Emplace(50);
    values.Emplace(60);
    TArray<int32> indices{1, 0, 3, 4, 2, 5};
    auto const expected_indices{indices};

    ml::apply_permutation(values, indices);

    REQUIRE(values.Num() == 6);
    CHECK(values[0].value == 20);
    CHECK(values[1].value == 10);
    CHECK(values[2].value == 40);
    CHECK(values[3].value == 50);
    CHECK(values[4].value == 30);
    CHECK(values[5].value == 60);
    CHECK(indices == expected_indices);
}

TEST_CASE("SandboxCore.SoaPermutation.ReusesPermutationAcrossParallelStreams") {
    TArray<int32> ids{10, 20, 30, 40};
    TArray<FString> names{TEXT("ten"), TEXT("twenty"), TEXT("thirty"), TEXT("forty")};
    TArray<int32> indices{2, 0, 3, 1};
    auto const expected_indices{indices};

    ml::apply_permutation(ids, indices);
    ml::apply_permutation(names, indices);

    CHECK((ids == TArray<int32>{30, 10, 40, 20}));
    CHECK((names == TArray<FString>{TEXT("thirty"), TEXT("ten"), TEXT("forty"), TEXT("twenty")}));
    CHECK(indices == expected_indices);
}

TEST_CASE("SandboxCore.SoaPermutation.EmptyAndIdentityInputs") {
    TArray<int32> empty_values;
    TArray<int32> empty_indices;
    ml::apply_permutation(empty_values, empty_indices);

    TArray<int32> values{10, 20, 30};
    TArray<int32> indices{0, 1, 2};
    ml::apply_permutation(values, indices);

    CHECK(empty_values.IsEmpty());
    CHECK(empty_indices.IsEmpty());
    CHECK((values == TArray<int32>{10, 20, 30}));
    CHECK((indices == TArray<int32>{0, 1, 2}));
}
