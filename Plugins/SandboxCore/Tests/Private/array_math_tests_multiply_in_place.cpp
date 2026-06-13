#include <SandboxCore/array_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.multiply_in_place.Zero") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{0, 0, 0};

    ml::multiply_in_place(array, 0);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.multiply_in_place.One") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{0, 1, 2};

    ml::multiply_in_place(array, 1);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.multiply_in_place.Two") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{0, 2, 4};

    ml::multiply_in_place(array, 2);
    REQUIRE(array == expected);
}
