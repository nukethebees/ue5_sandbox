#include "SandboxCore/rotation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.ShortestSignedAngleDelta returns zero when angles are equal",
          "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    CHECK(ml::shortest_signed_angle_delta_degrees(0.0f, 0.0f) == 0.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(90.0f, 90.0f) == 0.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(360.0f, 0.0f) == 0.0f);
}

TEST_CASE("SandboxCore.Math.ShortestSignedAngleDelta returns positive clockwise deltas",
          "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    CHECK(ml::shortest_signed_angle_delta_degrees(0.0f, 10.0f) == 10.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(45.0f, 90.0f) == 45.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(270.0f, 315.0f) == 45.0f);
}

TEST_CASE("SandboxCore.Math.ShortestSignedAngleDelta returns negative counter-clockwise deltas",
          "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    CHECK(ml::shortest_signed_angle_delta_degrees(10.0f, 0.0f) == -10.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(90.0f, 45.0f) == -45.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(315.0f, 270.0f) == -45.0f);
}

TEST_CASE("SandboxCore.Math.ShortestSignedAngleDelta wraps across zero degrees",
          "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    CHECK(ml::shortest_signed_angle_delta_degrees(350.0f, 10.0f) == 20.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(10.0f, 350.0f) == -20.0f);

    CHECK(ml::shortest_signed_angle_delta_degrees(359.0f, 1.0f) == 2.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(1.0f, 359.0f) == -2.0f);
}

TEST_CASE("SandboxCore.Math.ShortestSignedAngleDelta handles negative angles",
          "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    CHECK(ml::shortest_signed_angle_delta_degrees(-10.0f, 10.0f) == 20.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(10.0f, -10.0f) == -20.0f);

    CHECK(ml::shortest_signed_angle_delta_degrees(-350.0f, -10.0f) == -20.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(-10.0f, -350.0f) == 20.0f);
}

TEST_CASE("SandboxCore.Math.ShortestSignedAngleDelta handles angles outside one full turn",
          "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    CHECK(ml::shortest_signed_angle_delta_degrees(0.0f, 370.0f) == 10.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(370.0f, 0.0f) == -10.0f);

    CHECK(ml::shortest_signed_angle_delta_degrees(720.0f, 90.0f) == 90.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(-720.0f, -90.0f) == -90.0f);
}

TEST_CASE(
    "SandboxCore.Math.ShortestSignedAngleDelta normalises exact half-turns to negative half-turn",
    "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    CHECK(ml::shortest_signed_angle_delta_degrees(0.0f, 180.0f) == -180.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(180.0f, 0.0f) == -180.0f);

    CHECK(ml::shortest_signed_angle_delta_degrees(90.0f, 270.0f) == -180.0f);
    CHECK(ml::shortest_signed_angle_delta_degrees(270.0f, 90.0f) == -180.0f);
}

TEST_CASE("SandboxCore.Math.ShortestSignedAngleDelta supports double precision",
          "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    CHECK(ml::shortest_signed_angle_delta_degrees(350.0, 10.0) == 20.0);
    CHECK(ml::shortest_signed_angle_delta_degrees(10.0, 350.0) == -20.0);
    CHECK(ml::shortest_signed_angle_delta_degrees(0.0, 180.0) == -180.0);
}
