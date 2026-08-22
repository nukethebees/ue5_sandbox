#include <SandboxCore/timing.h>

#include "TestHarness.h"

TEST_CASE("SandboxCore.Timing.valid_periods.Accepts positive integral periods") {
    REQUIRE(ml::valid_periods(1, 2, 3));
}

TEST_CASE("SandboxCore.Timing.valid_periods.Accepts positive floating point periods") {
    REQUIRE(ml::valid_periods(0.5f, 1.0, 2.5));
}

TEST_CASE("SandboxCore.Timing.valid_periods.Accepts mixed positive periods") {
    REQUIRE(ml::valid_periods(1, 0.5f, 2.0));
}

TEST_CASE("SandboxCore.Timing.valid_periods.Rejects zero") {
    REQUIRE_FALSE(ml::valid_periods(1, 0, 2));
}

TEST_CASE("SandboxCore.Timing.valid_periods.Rejects negative periods") {
    REQUIRE_FALSE(ml::valid_periods(1.0, -0.5f, 2));
}
