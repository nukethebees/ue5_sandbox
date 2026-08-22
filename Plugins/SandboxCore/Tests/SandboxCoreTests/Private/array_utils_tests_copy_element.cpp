#include <SandboxCore/container_ops.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Array.CopyElement.IndexedContainer") {
    TArray<int32> dst{10, 20, 30};
    TArray<int32> const src{40, 50, 60};

    ml::CopyElementTraits<TArray<int32>>::copy_element(dst, 1, src, 2);

    TArray<int32> const expected_dst{10, 60, 30};
    TArray<int32> const expected_src{40, 50, 60};

    CHECK(dst == expected_dst);
    CHECK(src == expected_src);
}

TEST_CASE("SandboxCore.Array.CopyElements.IndexedContainer") {
    TArray<int32> dst{10, 20, 30, 40, 50};
    TArray<int32> const src{60, 70, 80, 90, 100};

    ml::copy_elements(dst, 0, src, 0, src.Num());
    CHECK(dst == src);

    TArray<int32> const subrange_src{1, 2, 3, 4, 5};
    ml::copy_elements(dst, 1, subrange_src, 2, 2);
    CHECK((dst == TArray<int32>{60, 3, 4, 90, 100}));

    auto const expected{dst};
    ml::copy_elements(dst, dst.Num(), dst, dst.Num(), 0);
    CHECK(dst == expected);
}
