#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.num.Default array") {
    TArray<int32> array{};

    REQUIRE(ml::num(array) == 0);
}
TEST_CASE("SandboxCore.Array.num.One value") {
    TArray<int32> array{0};

    REQUIRE(ml::num(array) == 1);
}
