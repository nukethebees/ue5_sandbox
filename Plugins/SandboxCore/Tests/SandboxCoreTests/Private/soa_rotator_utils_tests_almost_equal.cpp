#include <SandboxCore/soa_rotator_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaRotatorUtils.almost_equal.Empty") {
    FRotatorsf const a;
    FRotatorsf const b;

    CHECK(ml::almost_equal(a, b));
}

TEST_CASE("SandboxCore.SoaRotatorUtils.almost_equal.Equal") {
    auto const a{ml::make_rotatorsf({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};
    auto const b{ml::make_rotatorsf({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    CHECK(ml::almost_equal(a, b));
}

TEST_CASE("SandboxCore.SoaRotatorUtils.almost_equal.WithTolerance") {
    auto const a{ml::make_rotatorsf({1.0f}, {2.0f}, {3.0f})};
    auto const b{ml::make_rotatorsf({1.1f}, {2.0f}, {3.0f})};

    CHECK(ml::almost_equal(a, b, 0.2f));
}

TEST_CASE("SandboxCore.SoaRotatorUtils.almost_equal.Different") {
    auto const a{ml::make_rotatorsf({1.0f}, {2.0f}, {3.0f})};
    auto const b{ml::make_rotatorsf({1.0f}, {2.0f}, {4.0f})};

    CHECK(!ml::almost_equal(a, b, 0.2f));
}

TEST_CASE("SandboxCore.SoaRotatorUtils.almost_equal.DifferentNum") {
    auto const a{ml::make_rotatorsf({1.0f}, {2.0f}, {3.0f})};
    auto const b{ml::make_rotatorsf({1.0f, 2.0f}, {2.0f, 3.0f}, {3.0f, 4.0f})};

    CHECK(!ml::almost_equal(a, b));
}
