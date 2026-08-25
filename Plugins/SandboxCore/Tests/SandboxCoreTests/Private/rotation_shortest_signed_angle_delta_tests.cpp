#include "SandboxCore/rotation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
template <typename T>
struct TAngleDeltaCase {
    char const* name;
    T current;
    T target;
    T expected;
};
}

TEST_CASE("SandboxCore.Math.shortest_signed_angle_delta_degrees.float cases", "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    constexpr TAngleDeltaCase<float> cases[]{
        {"equal zero", 0.0f, 0.0f, 0.0f},
        {"equal positive", 90.0f, 90.0f, 0.0f},
        {"equivalent full turn", 360.0f, 0.0f, 0.0f},
        {"small clockwise delta", 0.0f, 10.0f, 10.0f},
        {"quarter clockwise delta", 45.0f, 90.0f, 45.0f},
        {"upper-quadrant clockwise delta", 270.0f, 315.0f, 45.0f},
        {"small counter-clockwise delta", 10.0f, 0.0f, -10.0f},
        {"quarter counter-clockwise delta", 90.0f, 45.0f, -45.0f},
        {"upper-quadrant counter-clockwise delta", 315.0f, 270.0f, -45.0f},
        {"clockwise across zero", 350.0f, 10.0f, 20.0f},
        {"counter-clockwise across zero", 10.0f, 350.0f, -20.0f},
        {"small clockwise crossing", 359.0f, 1.0f, 2.0f},
        {"small counter-clockwise crossing", 1.0f, 359.0f, -2.0f},
        {"negative current across zero", -10.0f, 10.0f, 20.0f},
        {"negative target across zero", 10.0f, -10.0f, -20.0f},
        {"both negative counter-clockwise", -350.0f, -10.0f, -20.0f},
        {"both negative clockwise", -10.0f, -350.0f, 20.0f},
        {"target above one turn", 0.0f, 370.0f, 10.0f},
        {"current above one turn", 370.0f, 0.0f, -10.0f},
        {"current two turns above", 720.0f, 90.0f, 90.0f},
        {"current two turns below", -720.0f, -90.0f, -90.0f},
        {"positive half-turn from zero", 0.0f, 180.0f, -180.0f},
        {"negative half-turn to zero", 180.0f, 0.0f, -180.0f},
        {"positive half-turn from quarter", 90.0f, 270.0f, -180.0f},
        {"negative half-turn from upper quarter", 270.0f, 90.0f, -180.0f},
    };

    for (auto const& test_case : cases) {
        DYNAMIC_SECTION(test_case.name) {
            CHECK(ml::shortest_signed_angle_delta_degrees(test_case.current, test_case.target) == test_case.expected);
        }
    }
}

TEST_CASE("SandboxCore.Math.shortest_signed_angle_delta_degrees.double precision", "[SandboxCore][Math][ShortestSignedAngleDelta]") {
    constexpr TAngleDeltaCase<double> cases[]{
        {"clockwise across zero", 350.0, 10.0, 20.0},
        {"counter-clockwise across zero", 10.0, 350.0, -20.0},
        {"exact half-turn", 0.0, 180.0, -180.0},
    };

    for (auto const& test_case : cases) {
        DYNAMIC_SECTION(test_case.name) {
            CHECK(ml::shortest_signed_angle_delta_degrees(test_case.current, test_case.target) == test_case.expected);
        }
    }
}
