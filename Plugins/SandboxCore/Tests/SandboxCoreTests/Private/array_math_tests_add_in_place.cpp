#include <SandboxCore/array_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.add_in_place.Zero") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{0, 1, 2};

    ml::add_in_place(array, 0);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.add_in_place.One") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{1, 2, 3};

    ml::add_in_place(array, 1);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.add_in_place.Negative") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{-2, -1, 0};

    ml::add_in_place(array, -2);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.add_in_place.MixedSigns") {
    TArray<int32> array{-3, 0, 4};
    TArray<int32> expected{2, 5, 9};

    ml::add_in_place(array, 5);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.add_in_place.Empty") {
    TArray<int32> array{};
    TArray<int32> expected{};

    ml::add_in_place(array, 1);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.add_in_place.Float") {
    TArray<float> array{-1.5f, 0.f, 2.25f};
    TArray<float> expected{-1.f, 0.5f, 2.75f};

    ml::add_in_place(array, 0.5f);
    REQUIRE(array == expected);
}

TEST_CASE("SandboxCore.Array.add_in_place.ArrayView") {
    TArray<int32> array{0, 1, 2};
    TArray<int32> expected{3, 4, 5};

    ml::add_in_place(TArrayView<int32>{array}, 3);
    REQUIRE(array == expected);
}
