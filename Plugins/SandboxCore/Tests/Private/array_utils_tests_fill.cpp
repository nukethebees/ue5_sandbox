#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.fill_first.Fills prefix") {
    TArray<int32> values{1, 2, 3, 4};

    ml::fill_first(values, 9, 2);

    REQUIRE((values == TArray<int32>{9, 9, 3, 4}));
}

TEST_CASE("SandboxCore.Array.fill_first.Zero count") {
    TArray<int32> values{1, 2, 3};

    ml::fill_first(values, 9, 0);

    REQUIRE((values == TArray<int32>{1, 2, 3}));
}

TEST_CASE("SandboxCore.Array.fill_last.Fills suffix") {
    TArray<int32> values{1, 2, 3, 4};

    ml::fill_last(values, 9, 2);

    REQUIRE((values == TArray<int32>{1, 2, 9, 9}));
}

TEST_CASE("SandboxCore.Array.fill_last.Zero count") {
    TArray<int32> values{1, 2, 3};

    ml::fill_last(values, 9, 0);

    REQUIRE((values == TArray<int32>{1, 2, 3}));
}
