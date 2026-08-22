#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.size.Zero") {
    auto const result{ml::size(0.0f, 0.0f, 0.0f)};

    REQUIRE(result == 0.0f);
}

TEST_CASE("SandboxCore.Math.size.OneAxis") {
    auto const result{ml::size(3.0f, 0.0f, 0.0f)};

    REQUIRE(result == 3.0f);
}

TEST_CASE("SandboxCore.Math.size.ThreeAxes") {
    auto const result{ml::size(3.0f, 4.0f, 12.0f)};

    REQUIRE(result == 13.0f);
}
