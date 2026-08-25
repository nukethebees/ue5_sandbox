#include "SandboxCore/rotation.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
struct FNormalisedAngleDeltaCase {
    char const* name;
    float current;
    float target;
    float expected;
};
}

TEST_CASE("SandboxCore.Math.ShortestDeltaNormalised.named cases", "[SandboxCore][Math][ShortestDeltaNormalised]") {
    constexpr FNormalisedAngleDeltaCase cases[]{
        {"equal zero", 0.0f, 0.0f, 0.0f},
        {"equal quarter turn", 90.0f, 90.0f, 0.0f},
        {"equal near full turn", 359.0f, 359.0f, 0.0f},
        {"small positive delta", 0.0f, 10.0f, 10.0f},
        {"quarter positive delta", 90.0f, 135.0f, 45.0f},
        {"upper-quadrant positive delta", 270.0f, 315.0f, 45.0f},
        {"small negative delta", 10.0f, 0.0f, -10.0f},
        {"quarter negative delta", 135.0f, 90.0f, -45.0f},
        {"upper-quadrant negative delta", 315.0f, 270.0f, -45.0f},
        {"long positive path wraps negative", 0.0f, 270.0f, -90.0f},
        {"positive zero crossing wraps negative", 10.0f, 350.0f, -20.0f},
        {"positive diagonal path wraps negative", 45.0f, 315.0f, -90.0f},
        {"long negative path wraps positive", 270.0f, 0.0f, 90.0f},
        {"negative zero crossing wraps positive", 350.0f, 10.0f, 20.0f},
        {"negative diagonal path wraps positive", 315.0f, 45.0f, 90.0f},
        {"positive half-turn from zero", 0.0f, 180.0f, -180.0f},
        {"positive half-turn from quarter", 90.0f, 270.0f, -180.0f},
        {"negative half-turn to zero", 180.0f, 0.0f, -180.0f},
        {"negative half-turn to quarter", 270.0f, 90.0f, -180.0f},
        {"small clockwise zero crossing", 359.0f, 1.0f, 2.0f},
        {"small counter-clockwise zero crossing", 1.0f, 359.0f, -2.0f},
        {"clockwise zero crossing", 355.0f, 5.0f, 10.0f},
        {"counter-clockwise zero crossing", 5.0f, 355.0f, -10.0f},
        {"zero and full turn are equivalent", 0.0f, 360.0f, 0.0f},
        {"full turn and zero are equivalent", 360.0f, 0.0f, 0.0f},
    };

    for (auto const& test_case : cases) {
        DYNAMIC_SECTION(test_case.name) {
            CHECK(ml::shortest_signed_angle_delta_degrees_normalised(test_case.current, test_case.target) == test_case.expected);
        }
    }
}
