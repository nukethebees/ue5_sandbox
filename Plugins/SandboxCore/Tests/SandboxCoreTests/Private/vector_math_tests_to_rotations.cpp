#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

namespace {
struct FToRotationCase {
    char const* name;
    float x;
    float y;
    float z;
    float expected_pitch;
    float expected_yaw;
};

constexpr FToRotationCase rotation_cases[]{
    {"forward", 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"right", 0.0f, 1.0f, 0.0f, 0.0f, 90.0f},
    {"up", 0.0f, 0.0f, 1.0f, 90.0f, 0.0f},
    {"diagonal", 1.0f, 1.0f, 0.0f, 0.0f, 45.0f},
    {"backward", -1.0f, 0.0f, 0.0f, 0.0f, 180.0f},
    {"left", 0.0f, -1.0f, 0.0f, 0.0f, -90.0f},
    {"up and forward", 1.0f, 0.0f, 1.0f, 45.0f, 0.0f},
};

auto nearly_equal(float const a, float const b) -> bool {
    return FMath::IsNearlyEqual(a, b, KINDA_SMALL_NUMBER);
}
}

TEST_CASE("SandboxCore.Math.to_rotation.Named scalar cases") {
    for (auto const& test_case : rotation_cases) {
        DYNAMIC_SECTION(test_case.name) {
            auto const result{ml::to_rotation(test_case.x, test_case.y, test_case.z)};

            CHECK(nearly_equal(static_cast<float>(result.Pitch), test_case.expected_pitch));
            CHECK(nearly_equal(static_cast<float>(result.Yaw), test_case.expected_yaw));
            CHECK(nearly_equal(static_cast<float>(result.Roll), 0.0f));
        }
    }
}

TEST_CASE("SandboxCore.Math.to_rotations.Pointer") {
    TArray<float> xs;
    TArray<float> ys;
    TArray<float> zs;
    TArray<float> expected_pitches;
    TArray<float> expected_yaws;
    for (auto const& test_case : rotation_cases) {
        xs.Add(test_case.x);
        ys.Add(test_case.y);
        zs.Add(test_case.z);
        expected_pitches.Add(test_case.expected_pitch);
        expected_yaws.Add(test_case.expected_yaw);
    }

    TArray<float> pitches;
    TArray<float> yaws;
    TArray<float> rolls;
    pitches.SetNumUninitialized(xs.Num());
    yaws.SetNumUninitialized(xs.Num());
    rolls.SetNumUninitialized(xs.Num());

    ml::kernel::to_rotations(pitches.GetData(), yaws.GetData(), rolls.GetData(), xs.GetData(), ys.GetData(), zs.GetData(), xs.Num());

    auto const result{ml::make_rotatorsf(pitches, yaws, rolls)};
    TArray<float> expected_rolls;
    expected_rolls.Init(0.0f, expected_pitches.Num());
    auto const expected{ml::make_rotatorsf(expected_pitches, expected_yaws, expected_rolls)};

    CHECK(ml::almost_equal(result, expected));
}

TEST_CASE("SandboxCore.Math.to_rotations.PointerEmpty") {
    TArray<float> const xs;
    TArray<float> const ys;
    TArray<float> const zs;
    TArray<float> pitches;
    TArray<float> yaws;
    TArray<float> rolls;

    ml::kernel::to_rotations(pitches.GetData(), yaws.GetData(), rolls.GetData(), xs.GetData(), ys.GetData(), zs.GetData(), xs.Num());

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
