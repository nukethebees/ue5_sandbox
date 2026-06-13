#include <SandboxCore/array_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.divide_in_place.One") {
    TArray<float> array{0.f, 1.f};
    TArray<float> expected{0.f, 1.f};

    ml::divide_in_place(array, 1.f);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.divide_in_place.Two") {
    TArray<double> array{0, 1, 2};
    TArray<double> expected{0, 0.5, 1};

    ml::divide_in_place(array, 2.0);
    REQUIRE(array == expected);
}
