#include "SandboxCore/array_utils.h"

#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.remove_at_swap_many_sorted_desc removes from one int32 array",
          "[SandboxCore][Array]") {
    TArray<int32> values{10, 20, 30, 40, 50, 60};
    TArray<int32> const indices{4, 2, 0};

    ml::remove_at_swap_many_sorted_desc(indices, values);

    REQUIRE(values.Num() == 3);
    CHECK(values[0] == 40);
    CHECK(values[1] == 20);
    CHECK(values[2] == 60);
}

TEST_CASE("SandboxCore.Array.remove_at_swap_many_sorted_desc removes from paired int32 and float arrays",
          "[SandboxCore][Array]") {
    TArray<int32> ids{10, 20, 30, 40, 50, 60};
    TArray<float> weights{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    TArray<int32> const indices{4, 2, 0};

    ml::remove_at_swap_many_sorted_desc(indices, ids, weights);

    REQUIRE(ids.Num() == 3);
    REQUIRE(weights.Num() == 3);

    CHECK(ids[0] == 40);
    CHECK(ids[1] == 20);
    CHECK(ids[2] == 60);

    CHECK(weights[0] == 4.0f);
    CHECK(weights[1] == 2.0f);
    CHECK(weights[2] == 6.0f);
}
