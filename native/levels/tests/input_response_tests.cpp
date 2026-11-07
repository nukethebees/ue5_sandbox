#include "ioj/input/response.h"

#include <gtest/gtest.h>

#include <limits>

namespace ioj::input::tests {
TEST(NativeInputResponse, RemapsScalarDeadZoneAndAppliesSelectedDeviceInversion) {
    GameSettingsState const settings{.mouse_turn_sensitivity = 0.5f,
                                     .gamepad_turn_sensitivity = 2.0f,
                                     .gamepad_turn_dead_zone = 0.25f,
                                     .gamepad_move_dead_zone = 0.5f,
                                     .invert_mouse_pitch = true,
                                     .invert_gamepad_yaw = true};
    EXPECT_FLOAT_EQ(apply_response(0.25f, Response::GamepadTurn, Axis::Yaw, settings), 0.0f);
    EXPECT_FLOAT_EQ(apply_response(0.625f, Response::GamepadTurn, Axis::Yaw, settings), -1.0f);
    EXPECT_FLOAT_EQ(apply_response(-0.625f, Response::GamepadTurn, Axis::Pitch, settings), -1.0f);
    EXPECT_FLOAT_EQ(apply_response(0.75f, Response::GamepadMove, Axis::Pitch, settings), 0.5f);
    EXPECT_FLOAT_EQ(apply_response(400.0f, Response::TurnPointerDelta, Axis::Pitch, settings),
                    -0.5f);
    EXPECT_FLOAT_EQ(apply_response(400.0f, Response::TurnPointerDelta, Axis::Yaw, settings), 0.5f);
}

TEST(NativeInputResponse, RadialDeadZonePreservesDirectionSaturatesAndInvertsOnlyY) {
    GameSettingsState const settings{.gamepad_turn_sensitivity = 2.0f,
                                     .gamepad_turn_dead_zone = 0.25f,
                                     .invert_gamepad_pitch = true};
    EXPECT_EQ(apply_response(Vector2d{0.1, 0.1}, Response::GamepadTurn, Axis::Pitch, settings),
              Vector2d{});
    auto const remapped{
        apply_response(Vector2d{0.3, 0.4}, Response::GamepadTurn, Axis::Pitch, settings)};
    EXPECT_NEAR(remapped.x(), 0.4, 1.e-12);
    EXPECT_NEAR(remapped.y(), -8.0 / 15.0, 1.e-12);
    auto const saturated{
        apply_response(Vector3d{0, 3, 4}, Response::GamepadTurn, Axis::Pitch, settings)};
    EXPECT_DOUBLE_EQ(saturated.x(), 0.0);
    EXPECT_NEAR(saturated.y(), -1.2, 1.e-12);
    EXPECT_NEAR(saturated.z(), 1.6, 1.e-12);
}

TEST(NativeInputResponse, ClampsTuningWithoutRestrictingPointerMagnitude) {
    GameSettingsState const settings{.mouse_turn_sensitivity = 2.0f,
                                     .gamepad_turn_sensitivity = 1.0f,
                                     .gamepad_turn_dead_zone = clamp_dead_zone(1.0f)};
    EXPECT_FLOAT_EQ(apply_response(800.0f, Response::TurnPointerDelta, Axis::None, settings), 4.0f);
    EXPECT_FLOAT_EQ(apply_response(1.0f, Response::GamepadTurn, Axis::None, settings), 1.0f);
    EXPECT_EQ(apply_response(Vector3d{}, Response::GamepadMove, Axis::None, settings), Vector3d{});
    EXPECT_FLOAT_EQ(clamp_sensitivity(-2.f), 0.f);
    EXPECT_FLOAT_EQ(clamp_dead_zone(-1.f), 0.f);
    for (auto const invalid : {std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity(),
                               -std::numeric_limits<float>::infinity()}) {
        EXPECT_FLOAT_EQ(clamp_sensitivity(invalid), 0.f);
        EXPECT_FLOAT_EQ(clamp_dead_zone(invalid), 0.f);
    }
}
}
