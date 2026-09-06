#include <SandboxCore/array_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.multiply.Zero") {
    TArray<int32> const input{-2, 0, 3};
    TArray<int32> output{1, 1, 1};
    TArray<int32> const expected{0, 0, 0};

    ml::multiply(input, 0, output);
    REQUIRE(output == expected);
}

TEST_CASE("SandboxCore.Array.multiply.MixedSigns") {
    TArray<int32> const input{-2, 0, 3};
    TArray<int32> output{0, 0, 0};
    TArray<int32> const expected{4, 0, -6};

    ml::multiply(input, -2, output);
    REQUIRE(output == expected);
}

TEST_CASE("SandboxCore.Array.multiply.Float") {
    TArray<float> const input{-1.5f, 0.f, 2.25f};
    TArray<float> output{0.f, 0.f, 0.f};
    TArray<float> const expected{-0.75f, 0.f, 1.125f};

    ml::multiply(input, 0.5f, output);
    REQUIRE(output == expected);
}

TEST_CASE("SandboxCore.Array.multiply.ZeroCount") {
    TArray<int32> const input{1, 2, 3};
    TArray<int32> output{4, 5, 6};
    TArray<int32> const expected{4, 5, 6};

    ml::multiply(TConstArrayView<int32>{input.GetData(), 0}, 2, TArrayView<int32>{output.GetData(), 0});
    REQUIRE(output == expected);
}

TEST_CASE("SandboxCore.Array.multiply.ArrayArray") {
    TArray<int32> const lhs{-2, 0, 3};
    TArray<int32> const rhs{4, 5, -6};
    TArray<int32> output{0, 0, 0};
    TArray<int32> const expected{-8, 0, -18};

    ml::multiply(lhs, rhs, output);
    REQUIRE(output == expected);
}
