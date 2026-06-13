#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaVectorUtils.almost_equal.Empty") {
    FVectors3f const a;
    FVectors3f const b;

    CHECK(ml::almost_equal(a, b));
}

TEST_CASE("SandboxCore.SoaVectorUtils.almost_equal.Equal") {
    FVectors3f a;
    a.xs = Values{1.0f, 2.0f};
    a.ys = Values{3.0f, 4.0f};
    a.zs = Values{5.0f, 6.0f};

    FVectors3f b;
    b.xs = Values{1.0f, 2.0f};
    b.ys = Values{3.0f, 4.0f};
    b.zs = Values{5.0f, 6.0f};

    CHECK(ml::almost_equal(a, b));
}

TEST_CASE("SandboxCore.SoaVectorUtils.almost_equal.WithTolerance") {
    FVectors3f a;
    a.xs = Values{1.0f};
    a.ys = Values{2.0f};
    a.zs = Values{3.0f};

    FVectors3f b;
    b.xs = Values{1.1f};
    b.ys = Values{2.0f};
    b.zs = Values{3.0f};

    CHECK(ml::almost_equal(a, b, 0.2f));
}

TEST_CASE("SandboxCore.SoaVectorUtils.almost_equal.Different") {
    FVectors3f a;
    a.xs = Values{1.0f};
    a.ys = Values{2.0f};
    a.zs = Values{3.0f};

    FVectors3f b;
    b.xs = Values{1.0f};
    b.ys = Values{2.0f};
    b.zs = Values{4.0f};

    CHECK(!ml::almost_equal(a, b, 0.2f));
}

TEST_CASE("SandboxCore.SoaVectorUtils.almost_equal.DifferentNum") {
    FVectors3f a;
    a.xs = Values{1.0f};
    a.ys = Values{2.0f};
    a.zs = Values{3.0f};

    FVectors3f b;
    b.xs = Values{1.0f, 2.0f};
    b.ys = Values{2.0f, 3.0f};
    b.zs = Values{3.0f, 4.0f};

    CHECK(!ml::almost_equal(a, b));
}
