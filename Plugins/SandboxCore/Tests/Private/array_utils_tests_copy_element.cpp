#include <SandboxCore/container_traits.h>

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
