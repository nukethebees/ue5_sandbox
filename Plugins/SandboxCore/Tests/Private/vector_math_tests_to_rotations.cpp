#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
auto nearly_equal(float const a, float const b) -> bool {
    return FMath::IsNearlyEqual(a, b, KINDA_SMALL_NUMBER);
}
}

TEST_CASE("SandboxCore.Math.to_rotation.Forward") {
    auto const result{ml::to_rotation(1.0f, 0.0f, 0.0f)};

    CHECK(nearly_equal(static_cast<float>(result.Pitch), 0.0f));
    CHECK(nearly_equal(static_cast<float>(result.Yaw), 0.0f));
    CHECK(nearly_equal(static_cast<float>(result.Roll), 0.0f));
}

TEST_CASE("SandboxCore.Math.to_rotation.Right") {
    auto const result{ml::to_rotation(0.0f, 1.0f, 0.0f)};

    CHECK(nearly_equal(static_cast<float>(result.Pitch), 0.0f));
    CHECK(nearly_equal(static_cast<float>(result.Yaw), 90.0f));
    CHECK(nearly_equal(static_cast<float>(result.Roll), 0.0f));
}

TEST_CASE("SandboxCore.Math.to_rotation.Up") {
    auto const result{ml::to_rotation(0.0f, 0.0f, 1.0f)};

    CHECK(nearly_equal(static_cast<float>(result.Pitch), 90.0f));
    CHECK(nearly_equal(static_cast<float>(result.Yaw), 0.0f));
    CHECK(nearly_equal(static_cast<float>(result.Roll), 0.0f));
}

TEST_CASE("SandboxCore.Math.to_rotation.Diagonal") {
    auto const result{ml::to_rotation(1.0f, 1.0f, 0.0f)};

    CHECK(nearly_equal(static_cast<float>(result.Pitch), 0.0f));
    CHECK(nearly_equal(static_cast<float>(result.Yaw), 45.0f));
    CHECK(nearly_equal(static_cast<float>(result.Roll), 0.0f));
}

TEST_CASE("SandboxCore.Math.to_rotation.Backward") {
    auto const result{ml::to_rotation(-1.0f, 0.0f, 0.0f)};

    CHECK(nearly_equal(static_cast<float>(result.Pitch), 0.0f));
    CHECK(nearly_equal(static_cast<float>(result.Yaw), 180.0f));
    CHECK(nearly_equal(static_cast<float>(result.Roll), 0.0f));
}

TEST_CASE("SandboxCore.Math.to_rotation.Left") {
    auto const result{ml::to_rotation(0.0f, -1.0f, 0.0f)};

    CHECK(nearly_equal(static_cast<float>(result.Pitch), 0.0f));
    CHECK(nearly_equal(static_cast<float>(result.Yaw), -90.0f));
    CHECK(nearly_equal(static_cast<float>(result.Roll), 0.0f));
}

TEST_CASE("SandboxCore.Math.to_rotation.UpAndForward") {
    auto const result{ml::to_rotation(1.0f, 0.0f, 1.0f)};

    CHECK(nearly_equal(static_cast<float>(result.Pitch), 45.0f));
    CHECK(nearly_equal(static_cast<float>(result.Yaw), 0.0f));
    CHECK(nearly_equal(static_cast<float>(result.Roll), 0.0f));
}

TEST_CASE("SandboxCore.Math.to_rotations.Pointer") {
    TArray<float> const xs{1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f};
    TArray<float> const ys{0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f};
    TArray<float> const zs{0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    TArray<float> pitches;
    TArray<float> yaws;
    TArray<float> rolls;
    pitches.SetNumUninitialized(xs.Num());
    yaws.SetNumUninitialized(xs.Num());
    rolls.SetNumUninitialized(xs.Num());

    ml::kernel::to_rotations(pitches.GetData(),
                             yaws.GetData(),
                             rolls.GetData(),
                             xs.GetData(),
                             ys.GetData(),
                             zs.GetData(),
                             xs.Num());

    auto const result{ml::make_rotatorsf(pitches, yaws, rolls)};
    auto const expected{ml::make_rotatorsf({0.0f, 0.0f, 90.0f, 0.0f, 0.0f, 0.0f, 45.0f},
                                           {0.0f, 90.0f, 0.0f, 45.0f, 180.0f, -90.0f, 0.0f},
                                           {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f})};

    CHECK(ml::almost_equal(result, expected));
}

TEST_CASE("SandboxCore.Math.to_rotations.PointerEmpty") {
    TArray<float> const xs;
    TArray<float> const ys;
    TArray<float> const zs;
    TArray<float> pitches;
    TArray<float> yaws;
    TArray<float> rolls;

    ml::kernel::to_rotations(pitches.GetData(),
                             yaws.GetData(),
                             rolls.GetData(),
                             xs.GetData(),
                             ys.GetData(),
                             zs.GetData(),
                             xs.Num());

    CHECK(pitches.IsEmpty());
    CHECK(yaws.IsEmpty());
    CHECK(rolls.IsEmpty());
}

TEST_CASE("SandboxCore.Math.to_rotations.ArrayView") {
    TArray<float> const xs{1.0f, 0.0f};
    TArray<float> const ys{0.0f, 1.0f};
    TArray<float> const zs{0.0f, 0.0f};
    TArray<float> pitches;
    TArray<float> yaws;
    TArray<float> rolls;
    pitches.SetNumUninitialized(xs.Num());
    yaws.SetNumUninitialized(xs.Num());
    rolls.SetNumUninitialized(xs.Num());

    ml::to_rotations(TArrayView<float>{pitches},
                     TArrayView<float>{yaws},
                     TArrayView<float>{rolls},
                     TConstArrayView<float>{xs},
                     TConstArrayView<float>{ys},
                     TConstArrayView<float>{zs});

    auto const result{ml::make_rotatorsf(pitches, yaws, rolls)};
    auto const expected{ml::make_rotatorsf({0.0f, 0.0f}, {0.0f, 90.0f}, {0.0f, 0.0f})};

    CHECK(ml::almost_equal(result, expected));
}
