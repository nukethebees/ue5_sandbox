#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using A = TArray<int32>;

TEST_CASE("SandboxCore.Array.all_num_equal.One empty array") {
    A array{};

    REQUIRE(ml::all_num_equal(array, array));
}
TEST_CASE("SandboxCore.Array.all_num_equal.Two empty arrays") {
    A array0{};
    A array1{};

    REQUIRE(ml::all_num_equal(array0, array1));
}
TEST_CASE("SandboxCore.Array.all_num_equal.Two different size") {
    REQUIRE(!ml::all_num_equal(A{}, A{1}));
}
TEST_CASE("SandboxCore.Array.all_num_equal.Three different size") {
    REQUIRE(!ml::all_num_equal(A{}, A{1}, A{2, 3}));
}
TEST_CASE("SandboxCore.Array.all_num_equal.Four equal") {
    REQUIRE(ml::all_num_equal(A{2}, A{1}, A{2}, A{4}));
}
