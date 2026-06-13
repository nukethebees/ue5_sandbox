#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.append_from.Vectors") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    FVectors3f to_append;
    to_append.xs = Values{4.0f, 7.0f};
    to_append.ys = Values{5.0f, 8.0f};
    to_append.zs = Values{6.0f, 9.0f};

    ml::append_from(vectors, to_append);

    Values const expected_xs{1.0f, 4.0f, 7.0f};
    Values const expected_ys{2.0f, 5.0f, 8.0f};
    Values const expected_zs{3.0f, 6.0f, 9.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.append_from.View") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    FVectors3f to_append;
    to_append.xs = Values{4.0f, 7.0f};
    to_append.ys = Values{5.0f, 8.0f};
    to_append.zs = Values{6.0f, 9.0f};

    ml::append_from(vectors, to_append.get_const_view().right(1));

    Values const expected_xs{1.0f, 7.0f};
    Values const expected_ys{2.0f, 8.0f};
    Values const expected_zs{3.0f, 9.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}

TEST_CASE("SandboxCore.SoaVectorUtils.append_element_from.Vectors") {
    FVectors3f vectors;
    vectors.xs = Values{1.0f};
    vectors.ys = Values{2.0f};
    vectors.zs = Values{3.0f};

    FVectors3f to_append;
    to_append.xs = Values{4.0f, 7.0f};
    to_append.ys = Values{5.0f, 8.0f};
    to_append.zs = Values{6.0f, 9.0f};

    ml::append_element_from(vectors, to_append, 1);

    Values const expected_xs{1.0f, 7.0f};
    Values const expected_ys{2.0f, 8.0f};
    Values const expected_zs{3.0f, 9.0f};

    CHECK(vectors.xs == expected_xs);
    CHECK(vectors.ys == expected_ys);
    CHECK(vectors.zs == expected_zs);
}
