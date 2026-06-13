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
TEST_CASE("SandboxCore.Array.fill.Three value array with negative value") {
    TArray<int32> array{999, 1000, 1001};
    ml::fill(array, -7);

    REQUIRE(array == A{-7, -7, -7});
    REQUIRE(array.Max() == 3);
}
TEST_CASE("SandboxCore.Array.fill.Does not change num or capacity") {
    TArray<int32> array{1, 2, 3};
    array.Reserve(8);

    auto const num{array.Num()};
    auto const max{array.Max()};

    ml::fill(array, 4);

    REQUIRE(array == A{4, 4, 4});
    REQUIRE(array.Num() == num);
    REQUIRE(array.Max() == max);
}
TEST_CASE("SandboxCore.Array.fill.Right view") {
    TArray<int32> array{1, 2, 3, 4};
    auto view{TArrayView<int32>{array}.Right(2)};

    ml::fill(view, 9);

    REQUIRE(array == A{1, 2, 9, 9});
    REQUIRE(array.Max() == 4);
}
TEST_CASE("SandboxCore.Array.fill.Empty right view") {
    TArray<int32> array{1, 2, 3};
    auto view{TArrayView<int32>{array}.Right(0)};

    ml::fill(view, 9);

    REQUIRE(array == A{1, 2, 3});
    REQUIRE(array.Max() == 3);
}
TEST_CASE("SandboxCore.Array.fill.Float array") {
    TArray<float> array{0.0f, 1.0f, 2.0f};
    ml::fill(array, 1.5f);

    REQUIRE(array == TArray<float>{1.5f, 1.5f, 1.5f});
    REQUIRE(array.Max() == 3);
}
