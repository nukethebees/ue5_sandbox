#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using A = TArray<int32>;

TEST_CASE("SandboxCore.Array.fill.Empty array") {
    TArray<int32> array{};
    ml::fill(array, 0);

    REQUIRE(array == A{});
    REQUIRE(array.Max() == 0);
}
TEST_CASE("SandboxCore.Array.fill.One value array") {
    TArray<int32> array{999};
    ml::fill(array, 0);

    REQUIRE(array == A{0});
    REQUIRE(array.Max() == 1);
}
TEST_CASE("SandboxCore.Array.fill.Two value array") {
    TArray<int32> array{999, 999};
    ml::fill(array, 10);

    REQUIRE(array == A{10, 10});
    REQUIRE(array.Max() == 2);
}
