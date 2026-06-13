#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.size_sq.Zero") {
    auto const result{ml::size_sq(0, 0, 0)};

    REQUIRE(result == 0);
}

TEST_CASE("SandboxCore.Math.size_sq.OneAxis") {
    auto const result{ml::size_sq(3, 0, 0)};

    REQUIRE(result == 9);
}

TEST_CASE("SandboxCore.Math.size_sq.ThreeAxes") {
    auto const result{ml::size_sq(3, 4, 12)};

    REQUIRE(result == 169);
}
