#include "SandboxNative/array.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxNative.map_array.Add one") {
    TArray input{1, 2, 3};
    TArray expected_output{2, 3, 4};

    constexpr auto fn{[](auto const value) { return value + 1; }};

    auto const out0{ml::map_array(input, fn)};
    auto const out1{ml::map_array<fn>(input)};

    REQUIRE(expected_output == out0);
    REQUIRE(expected_output == out1);
}
