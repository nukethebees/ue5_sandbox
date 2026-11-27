#include "ioj/input/logical/flight_input.h"

#include <gtest/gtest.h>

namespace ioj::tests {
TEST(NativeInputResponse, RemapsScalarDeadZoneAndAppliesSelectedDeviceInversion) {
    ControlsSettings const settings{.mouse_turn_sensitivity = 0.5f,
                                    .gamepad_turn_sensitivity = 2.0f,
                                    .gamepad_turn_dead_zone = 0.25f,
                                    .invert_mouse_pitch = true,
                                    .invert_gamepad_yaw = true};
    EXPECT_FLOAT_EQ(apply_response(0.25f, InputResponse::GamepadTurn, FlightAxis::Yaw, settings),
                    0.0f);
    EXPECT_FLOAT_EQ(apply_response(0.625f, InputResponse::GamepadTurn, FlightAxis::Yaw, settings),
                    -1.0f);
    EXPECT_FLOAT_EQ(
        apply_response(-0.625f, InputResponse::GamepadTurn, FlightAxis::Pitch, settings), -1.0f);
    EXPECT_FLOAT_EQ(
        apply_response(400.0f, InputResponse::TurnPointerDelta, FlightAxis::Pitch, settings),
        -0.5f);
    EXPECT_FLOAT_EQ(
        apply_response(400.0f, InputResponse::TurnPointerDelta, FlightAxis::Yaw, settings), 0.5f);
}

TEST(NativeInputResponse, RadialDeadZonePreservesDirectionSaturatesAndInvertsOnlyY) {
    ControlsSettings const settings{.gamepad_turn_sensitivity = 2.0f,
                                    .gamepad_turn_dead_zone = 0.25f,
                                    .invert_gamepad_pitch = true};
    EXPECT_EQ(
        apply_response(Vector2d{0.1, 0.1}, InputResponse::GamepadTurn, FlightAxis::Pitch, settings),
        Vector2d{});
    auto const remapped{apply_response(
        Vector2d{0.3, 0.4}, InputResponse::GamepadTurn, FlightAxis::Pitch, settings)};
    EXPECT_NEAR(remapped.x(), 0.4, 1.e-12);
    EXPECT_NEAR(remapped.y(), -8.0 / 15.0, 1.e-12);
    auto const saturated{
        apply_response(Vector3d{0, 3, 4}, InputResponse::GamepadTurn, FlightAxis::Pitch, settings)};
    EXPECT_DOUBLE_EQ(saturated.x(), 0.0);
    EXPECT_NEAR(saturated.y(), -1.2, 1.e-12);
    EXPECT_NEAR(saturated.z(), 1.6, 1.e-12);
}

TEST(NativeInputResponse, ClampsTuningWithoutRestrictingPointerMagnitude) {
    EXPECT_FLOAT_EQ(clamp_sensitivity(-2.0f), 0.0f);
    EXPECT_FLOAT_EQ(clamp_dead_zone(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(clamp_dead_zone(1.0f), 0.95f);
    ControlsSettings const settings{.mouse_turn_sensitivity = 2.0f,
                                    .gamepad_turn_sensitivity = 1.0f,
                                    .gamepad_turn_dead_zone = 1.0f};
    EXPECT_FLOAT_EQ(
        apply_response(800.0f, InputResponse::TurnPointerDelta, FlightAxis::None, settings), 4.0f);
    EXPECT_FLOAT_EQ(apply_response(1.0f, InputResponse::GamepadTurn, FlightAxis::None, settings),
                    1.0f);
    EXPECT_EQ(apply_response(Vector3d{}, InputResponse::GamepadTurn, FlightAxis::None, settings),
              Vector3d{});
}
}
