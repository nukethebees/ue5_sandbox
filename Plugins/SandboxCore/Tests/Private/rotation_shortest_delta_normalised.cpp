#include "SandboxCore/Public/rotation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised returns zero when angles are equal", "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(0.0f, 0.0f) == 0.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(90.0f, 90.0f) == 0.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(359.0f, 359.0f) == 0.0f);
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised returns positive delta when target is ahead",
          "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(0.0f, 10.0f) == 10.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(90.0f, 135.0f) == 45.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(270.0f, 315.0f) == 45.0f);
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised returns negative delta when target is behind",
          "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(10.0f, 0.0f) == -10.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(135.0f, 90.0f) == -45.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(315.0f, 270.0f) == -45.0f);
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised wraps positive deltas greater than half a turn",
          "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(0.0f, 270.0f) == -90.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(10.0f, 350.0f) == -20.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(45.0f, 315.0f) == -90.0f);
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised wraps negative deltas less than negative half a turn",
          "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(270.0f, 0.0f) == 90.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(350.0f, 10.0f) == 20.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(315.0f, 45.0f) == 90.0f);
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised maps positive half turn to negative half turn",
          "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(0.0f, 180.0f) == -180.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(90.0f, 270.0f) == -180.0f);
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised keeps negative half turn unchanged", "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(180.0f, 0.0f) == -180.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(270.0f, 90.0f) == -180.0f);
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised handles values near zero crossing", "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(359.0f, 1.0f) == 2.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(1.0f, 359.0f) == -2.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(355.0f, 5.0f) == 10.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(5.0f, 355.0f) == -10.0f);
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised assumes inputs are within one normalised turn",
          "[SandboxCore][Math][ShortestDeltaNormalised]") {
    CHECK(ml::shortest_signed_angle_delta_normalised(0.0f, 360.0f) == 0.0f);
    CHECK(ml::shortest_signed_angle_delta_normalised(360.0f, 0.0f) == 0.0f);
}
