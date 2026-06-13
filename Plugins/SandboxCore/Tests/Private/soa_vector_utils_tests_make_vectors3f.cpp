#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.make_vectors3f.InitializerLists") {
    auto const vectors{ml::make_vectors3f({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    Values const expected_xs{1.0f, 2.0f};
    Values const expected_ys{3.0f, 4.0f};
    Values const expected_zs{5.0f, 6.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.make_vectors3f.Arrays") {
    Values xs{1.0f, 2.0f};
    Values ys{3.0f, 4.0f};
    Values zs{5.0f, 6.0f};

    auto const vectors{ml::make_vectors3f(MoveTemp(xs), MoveTemp(ys), MoveTemp(zs))};

    Values const expected_xs{1.0f, 2.0f};
    Values const expected_ys{3.0f, 4.0f};
    Values const expected_zs{5.0f, 6.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.make_vectors3f.Empty") {
    auto const vectors{ml::make_vectors3f(std::initializer_list<float>{},
                                          std::initializer_list<float>{},
                                          std::initializer_list<float>{})};

    CHECK(vectors.num() == 0);
    CHECK(vectors.is_empty());
    CHECK(vectors.xs.IsEmpty());
    CHECK(vectors.ys.IsEmpty());
    CHECK(vectors.zs.IsEmpty());
}
