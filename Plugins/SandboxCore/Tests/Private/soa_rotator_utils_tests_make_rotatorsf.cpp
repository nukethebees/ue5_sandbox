#include <SandboxCore/soa_rotator_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

using Values = TArray<float>;

TEST_CASE("SandboxCore.SoaRotatorUtils.make_rotatorsf.InitializerLists") {
    auto const rotators{ml::make_rotatorsf({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f})};

    Values const expected_pitches{1.0f, 2.0f};
    Values const expected_yaws{3.0f, 4.0f};
    Values const expected_rolls{5.0f, 6.0f};

    CHECK(rotators.pitches == expected_pitches);
    CHECK(rotators.yaws == expected_yaws);
    CHECK(rotators.rolls == expected_rolls);
}

TEST_CASE("SandboxCore.SoaRotatorUtils.make_rotatorsf.Arrays") {
    Values pitches{1.0f, 2.0f};
    Values yaws{3.0f, 4.0f};
    Values rolls{5.0f, 6.0f};

    auto const rotators{ml::make_rotatorsf(MoveTemp(pitches), MoveTemp(yaws), MoveTemp(rolls))};

    Values const expected_pitches{1.0f, 2.0f};
    Values const expected_yaws{3.0f, 4.0f};
    Values const expected_rolls{5.0f, 6.0f};

    CHECK(rotators.pitches == expected_pitches);
    CHECK(rotators.yaws == expected_yaws);
    CHECK(rotators.rolls == expected_rolls);
}

TEST_CASE("SandboxCore.SoaRotatorUtils.make_rotatorsf.Empty") {
    auto const rotators{ml::make_rotatorsf(std::initializer_list<float>{},
                                           std::initializer_list<float>{},
                                           std::initializer_list<float>{})};

    CHECK(rotators.num() == 0);
    CHECK(rotators.is_empty());
    CHECK(rotators.pitches.IsEmpty());
    CHECK(rotators.yaws.IsEmpty());
    CHECK(rotators.rolls.IsEmpty());
}
