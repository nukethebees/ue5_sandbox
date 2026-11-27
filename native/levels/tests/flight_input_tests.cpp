#include <ioj/input/logical/flight_input.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

namespace ioj::tests {
TEST(FlightInput, MultipleDevicesProduceOneOrderedPressAndRelease) {
    ControlsSettings settings{};
    FlightInputState input;
    LogicalInputState state;
    input.reset(state);
    input.button(state,
                 PhysicalKey::LeftMouseButton,
                 ButtonState::Pressed,
                 settings,
                 InputContext::Gameplay);
    input.button(state,
                 PhysicalKey::GamepadRightTriggerAxis,
                 ButtonState::Pressed,
                 settings,
                 InputContext::Gameplay);
    input.button(state,
                 PhysicalKey::LeftMouseButton,
                 ButtonState::Released,
                 settings,
                 InputContext::Gameplay);
    ASSERT_EQ(state.transitions.size(), 1u);
    EXPECT_EQ(state.transitions.front().action, DigitalAction::Fire);
    EXPECT_EQ(state.transitions.front().state, ButtonState::Pressed);

    state.begin_frame();
    input.button(state,
                 PhysicalKey::GamepadRightTriggerAxis,
                 ButtonState::Released,
                 settings,
                 InputContext::Gameplay);
    input.button(
        state, PhysicalKey::LeftShift, ButtonState::Pressed, settings, InputContext::Gameplay);
    input.reset(state);
    ASSERT_EQ(state.transitions.size(), 3u);
    EXPECT_EQ(state.transitions[0].action, DigitalAction::Fire);
    EXPECT_EQ(state.transitions[0].state, ButtonState::Released);
    EXPECT_EQ(state.transitions[1].action, DigitalAction::Boost);
    EXPECT_EQ(state.transitions[1].state, ButtonState::Pressed);
    EXPECT_EQ(state.transitions[2].action, DigitalAction::Boost);
    EXPECT_EQ(state.transitions[2].state, ButtonState::Released);
}

TEST(FlightInput, EdgesBetweenTicksKeepTheirOrder) {
    ControlsSettings settings{};
    FlightInputState bindings;
    LogicalInputState state;
    bindings.reset(state);
    for (auto edge : {ButtonState::Pressed, ButtonState::Released, ButtonState::Pressed}) {
        bindings.button(
            state, PhysicalKey::LeftMouseButton, edge, settings, InputContext::Gameplay);
    }
    EXPECT_TRUE(state.held.fire());
    EXPECT_TRUE(std::ranges::equal(
        state.transitions,
        std::array{ButtonState::Pressed, ButtonState::Released, ButtonState::Pressed},
        {},
        &ActionTransition::state));
    state.begin_frame();
    EXPECT_TRUE(state.held.fire());
    EXPECT_TRUE(state.transitions.empty());
}

TEST(FlightInput, ReboundPauseAndViewerControlsAreIsolatedFromFlight) {
    ControlsSettings settings{};
    settings.bindings[FlightMapping::GeneralPauseKeyboardMouse] = PhysicalKey::P;
    FlightInputState input;
    LogicalInputState state;
    input.reset(state);
    input.button(
        state, PhysicalKey::Escape, ButtonState::Pressed, settings, InputContext::Observer);
    EXPECT_FALSE(state.held.pause());
    input.button(state, PhysicalKey::P, ButtonState::Pressed, settings, InputContext::Observer);
    EXPECT_TRUE(state.held.pause());
    input.button(state, PhysicalKey::P, ButtonState::Pressed, settings, InputContext::Observer);
    EXPECT_EQ(state.transitions.size(), 1u);
    input.button(state, PhysicalKey::W, ButtonState::Pressed, settings, InputContext::Observer);
    input.button(
        state, PhysicalKey::LeftShift, ButtonState::Pressed, settings, InputContext::Observer);
    ASSERT_EQ(state.transitions.size(), 1u);
    EXPECT_EQ(state.transitions.front().action, DigitalAction::Pause);
    EXPECT_EQ(state.axes_1d[Axis1DAction::Accelerate], 0.f);
    settings.bindings[FlightMapping::GeneralPauseKeyboardMouse] = PhysicalKey::NonUSHash;
    input.reset(state);
    input.button(state, PhysicalKey::NonUSHash, ButtonState::Pressed, settings, InputContext::Menu);
    EXPECT_TRUE(state.held.pause());
    for (auto key : {PhysicalKey::MouseScrollUp, PhysicalKey::MouseX}) {
        EXPECT_FALSE(accepts_binding(FlightMapping::GeneralPauseKeyboardMouse, key));
    }
}

TEST(FlightInput, TransientPointerMotionCannotHoldFireOrAcceleration) {
    using enum FlightMapping;
    EXPECT_TRUE(accepts_binding(FlightYawKeyboardMouse, PhysicalKey::MouseY));
    EXPECT_TRUE(accepts_binding(FlightAccelerateGamepad, PhysicalKey::GamepadFaceButtonTop));
    EXPECT_TRUE(accepts_binding(FlightFirePrimaryGamepad, PhysicalKey::GamepadRightTriggerAxis));
    EXPECT_TRUE(accepts_binding(FlightFirePrimaryKeyboardMouse, PhysicalKey::MouseScrollUp));
    EXPECT_FALSE(accepts_binding(FlightFirePrimaryKeyboardMouse, PhysicalKey::MouseX));
    EXPECT_FALSE(accepts_binding(FlightAccelerateKeyboardMouse, PhysicalKey::MouseY));
    EXPECT_FALSE(accepts_binding(FlightPitchKeyboardMouse, PhysicalKey::MouseWheelAxis));
    ControlsSettings controls{};
    FlightInputState flight;
    LogicalInputState state;
    flight.reset(state);
    flight.pointer(state, {10.f, 20.f}, controls);
    EXPECT_TRUE(state.transitions.empty());
    EXPECT_EQ(state.axes_1d[Axis1DAction::Accelerate], 0.f);
    flight.button(state,
                  PhysicalKey::LeftMouseButton,
                  ButtonState::Pressed,
                  controls,
                  InputContext::Gameplay);
    EXPECT_TRUE(state.held.fire());
}

TEST(FlightInput, PointerSteeringHandsBackToHeldStickAndClearsOnContextChange) {
    ControlsSettings settings{.mouse_turn_sensitivity = 1.f, .gamepad_turn_sensitivity = 1.f};
    FlightInputState input;
    LogicalInputState state;
    input.reset(state);
    input.axis(state, PhysicalKey::GamepadLeftX, -1.f, settings);
    input.button(state,
                 PhysicalKey::RightMouseButton,
                 ButtonState::Pressed,
                 settings,
                 InputContext::Gameplay);
    input.pointer(state, {200.f, 0.f}, settings);
    state.begin_frame();
    EXPECT_FLOAT_EQ(state.axes_2d[Axis2DAction::FlightTurn].x(), 0.5f);
    input.button(state,
                 PhysicalKey::RightMouseButton,
                 ButtonState::Released,
                 settings,
                 InputContext::Gameplay);
    EXPECT_FLOAT_EQ(state.axes_2d[Axis2DAction::FlightTurn].x(), -1.f);
    input.reset(state);
    EXPECT_EQ(state.axes_2d[Axis2DAction::FlightTurn].x(), 0.f);
    settings.bindings[FlightMapping::FlightYawKeyboardMouse] = PhysicalKey::J;
    input.reset(state);
    input.button(state, PhysicalKey::J, ButtonState::Pressed, settings, InputContext::Gameplay);
    input.button(state,
                 PhysicalKey::RightMouseButton,
                 ButtonState::Pressed,
                 settings,
                 InputContext::Gameplay);
    EXPECT_FLOAT_EQ(state.axes_2d[Axis2DAction::FlightTurn].x(), 1.f);
    state.begin_frame();
    EXPECT_FLOAT_EQ(state.axes_2d[Axis2DAction::FlightTurn].x(), 1.f);
    input.button(state, PhysicalKey::J, ButtonState::Released, settings, InputContext::Gameplay);
    EXPECT_FLOAT_EQ(state.axes_2d[Axis2DAction::FlightTurn].x(), 0.f);
}
}
