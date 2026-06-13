#include <SandboxCore/array_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.add_in_place.Add zero") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{0, 1, 2};

    REQUIRE(array == expected);
    ml::add_in_place(array, 0);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.add_in_place.Add one") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{1, 2, 3};

    REQUIRE(array != expected);
    ml::add_in_place(array, 1);
    REQUIRE(array == expected);
}
