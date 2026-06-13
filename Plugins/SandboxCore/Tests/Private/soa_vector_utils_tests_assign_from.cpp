#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.assign_from.Vectors") {
    FVectors3f dst;
    dst.xs = Values{1.0f};
    dst.ys = Values{2.0f};
    dst.zs = Values{3.0f};

    FVectors3f src;
    src.xs = Values{4.0f, 7.0f};
    src.ys = Values{5.0f, 8.0f};
    src.zs = Values{6.0f, 9.0f};

    ml::assign_from(dst, src);

    Values const expected_xs{4.0f, 7.0f};
    Values const expected_ys{5.0f, 8.0f};
    Values const expected_zs{6.0f, 9.0f};

    CHECK(dst.xs == expected_xs);
    CHECK(dst.ys == expected_ys);
    CHECK(dst.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign_from.Element") {
    FVectors3f dst;
    dst.xs = Values{1.0f, 2.0f};
    dst.ys = Values{3.0f, 4.0f};
    dst.zs = Values{5.0f, 6.0f};

    FVectors3f src;
    src.xs = Values{7.0f, 10.0f};
    src.ys = Values{8.0f, 11.0f};
    src.zs = Values{9.0f, 12.0f};

    ml::assign_from(dst, 0, src, 1);

    Values const expected_xs{10.0f, 2.0f};
    Values const expected_ys{11.0f, 4.0f};
    Values const expected_zs{12.0f, 6.0f};

    CHECK(dst.xs == expected_xs);
    CHECK(dst.ys == expected_ys);
    CHECK(dst.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign_from_scaled.Vectors") {
    FVectors3f dst;
    dst.set_num_uninitialized(2);

    FVectors3f src;
    src.xs = Values{1.0f, 2.0f};
    src.ys = Values{3.0f, 4.0f};
    src.zs = Values{5.0f, 6.0f};

    Values const scale_factors{2.0f, 3.0f};

    ml::assign_from_scaled(dst, src, scale_factors);

    Values const expected_xs{2.0f, 6.0f};
    Values const expected_ys{6.0f, 12.0f};
    Values const expected_zs{10.0f, 18.0f};

    CHECK(dst.xs == expected_xs);
    CHECK(dst.ys == expected_ys);
    CHECK(dst.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.assign_from_scaled.View") {
    FVectors3f dst;
    dst.set_num_uninitialized(1);

    FVectors3f src;
    src.xs = Values{1.0f, 2.0f};
    src.ys = Values{3.0f, 4.0f};
    src.zs = Values{5.0f, 6.0f};

    Values const scale_factors{3.0f};

    ml::assign_from_scaled(dst, src.get_const_view().right(1), scale_factors);

    Values const expected_xs{6.0f};
    Values const expected_ys{12.0f};
    Values const expected_zs{18.0f};

    CHECK(dst.xs == expected_xs);
    CHECK(dst.ys == expected_ys);
    CHECK(dst.zs == expected_zs);
}
