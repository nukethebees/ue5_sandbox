#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using A = TArray<int32>;

TEST_CASE("SandboxCore.Array.append_n.Empty array append 0x0") {
    TArray<int32> array{};
    ml::append_n(array, 0, 0);

    REQUIRE(array == A{});
}
TEST_CASE("SandboxCore.Array.append_n.Empty array append 0x1") {
    TArray<int32> array{};
    ml::append_n(array, 0, 1);

    REQUIRE(array == A{0});
}
TEST_CASE("SandboxCore.Array.append_n.Empty array append 1x1") {
    TArray<int32> array{};
    ml::append_n(array, 1, 1);

    REQUIRE(array == A{1});
}
TEST_CASE("SandboxCore.Array.append_n.Empty array append 2x1") {
    TArray<int32> array{};
    ml::append_n(array, 1, 2);

    REQUIRE(array == A{1, 1});
}
TEST_CASE("SandboxCore.Array.append_n.Two value array append 2x2") {
    TArray<int32> array{3, 3};
    ml::append_n(array, 2, 2);

    REQUIRE(array == A{3, 3, 2, 2});
}
