#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.add_scaled_in_place.Zero") {
    FVectors3f dst;
    dst.xs = Values{1.0f, 2.0f};
    dst.ys = Values{3.0f, 4.0f};
    dst.zs = Values{5.0f, 6.0f};

    FVectors3f src;
    src.xs = Values{1.0f, 1.0f};
    src.ys = Values{2.0f, 2.0f};
    src.zs = Values{3.0f, 3.0f};

    ml::add_scaled_in_place(dst, src, 0.0f);

    Values const expected_xs{1.0f, 2.0f};
    Values const expected_ys{3.0f, 4.0f};
    Values const expected_zs{5.0f, 6.0f};

    CHECK(dst.xs == expected_xs);
    CHECK(dst.ys == expected_ys);
    CHECK(dst.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.add_scaled_in_place.Two") {
    FVectors3f dst;
    dst.xs = Values{1.0f, 2.0f};
    dst.ys = Values{3.0f, 4.0f};
    dst.zs = Values{5.0f, 6.0f};

    FVectors3f src;
    src.xs = Values{1.0f, 1.0f};
    src.ys = Values{2.0f, 2.0f};
    src.zs = Values{3.0f, 3.0f};

    ml::add_scaled_in_place(dst, src, 2.0f);

    Values const expected_xs{3.0f, 4.0f};
    Values const expected_ys{7.0f, 8.0f};
    Values const expected_zs{11.0f, 12.0f};

    CHECK(dst.xs == expected_xs);
    CHECK(dst.ys == expected_ys);
    CHECK(dst.zs == expected_zs);
}
