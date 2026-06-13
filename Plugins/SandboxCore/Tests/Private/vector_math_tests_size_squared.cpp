#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.size_squared.Zero") {
    auto const result{ml::size_squared(0, 0, 0)};

    REQUIRE(result == 0);
}

TEST_CASE("SandboxCore.Math.size_squared.OneAxis") {
    auto const result{ml::size_squared(3, 0, 0)};

    REQUIRE(result == 9);
}

TEST_CASE("SandboxCore.Math.size_squared.ThreeAxes") {
    auto const result{ml::size_squared(3, 4, 12)};

    REQUIRE(result == 169);
}
