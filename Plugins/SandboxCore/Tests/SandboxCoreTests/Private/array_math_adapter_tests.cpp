#include <SandboxCore/array_math.h>
#include <SandboxCore/array_math_kernels.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.ArrayAdapter.Scalar kernels accept TArray") {
    TArray<int32> values{1, 2, 3};

    ml::add_in_place(values, 4);

    REQUIRE(values == TArray<int32>{5, 6, 7});
}

TEST_CASE("SandboxCore.ArrayAdapter.Scalar kernels accept TArrayView") {
    TArray<float> values{1.0f, 2.0f, 3.0f};

    ml::divide_in_place(TArrayView<float>{values}, 2.0f);

    REQUIRE(values == TArray<float>{0.5f, 1.0f, 1.5f});
}

TEST_CASE("SandboxCore.ArrayAdapter.Array kernels accept TArray views") {
    TArray<int32> lhs{1, 2, 3};
    TArray<int32> rhs{4, 5, 6};
    TArray<int32> output;
    output.SetNumUninitialized(lhs.Num());

    ml::multiply(TConstArrayView<int32>{lhs}, TConstArrayView<int32>{rhs}, TArrayView<int32>{output});

    REQUIRE(output == TArray<int32>{4, 10, 18});
}
