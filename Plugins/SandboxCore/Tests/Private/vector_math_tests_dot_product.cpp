#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.dot_product.Zero") {
    auto const result{ml::dot_product(0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f)};

    REQUIRE(result == 0.0f);
}

TEST_CASE("SandboxCore.Math.dot_product.One") {
    auto const result{ml::dot_product(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f)};

    REQUIRE(result == 3.0f);
}

TEST_CASE("SandboxCore.Math.dot_product.MixedSigns") {
    auto const result{ml::dot_product(1, -2, 3, 4, 5, -6)};

    REQUIRE(result == -24);
}
