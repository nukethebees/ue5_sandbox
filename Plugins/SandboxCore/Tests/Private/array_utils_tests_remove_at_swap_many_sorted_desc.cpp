#include "SandboxCore/array_utils.h"

#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.remove_at_swap_many_sorted_desc.Removes from one int32 array") {
    TArray<int32> values{00, 10, 20, 30, 40, 50, 60};
    TArray<int32> const indices{4, 2, 0};

    // 00, 10, 20, 30, 40, 50, 60
    // 00, 10, 20, 30, 60, 50
    // 00, 10, 50, 30, 60
    // 60, 10, 50, 30

    TArray<int32> expected{60, 10, 50, 30};

    ml::remove_at_swap_many_sorted_desc(indices, values);

    REQUIRE(values == expected);
}

TEST_CASE("SandboxCore.Array.remove_at_swap_many_sorted_desc.Removes from paired int32 and float arrays") {
    TArray<int32> ids{00, 10, 20, 30, 40, 50, 60};
    TArray<float> weights{0.f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

    TArray<int32> const indices{4, 2, 0};

    TArray<int32> expected_ids{60, 10, 50, 30};
    TArray<float> expected_weights{6.0f, 1.0f, 5.0f, 3.0f};

    ml::remove_at_swap_many_sorted_desc(indices, ids, weights);

    REQUIRE(ids == expected_ids);
    REQUIRE(weights == expected_weights);
}
