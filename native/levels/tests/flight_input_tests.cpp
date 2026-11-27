#include <ioj/input/dispatch/input_dispatcher.h>
#include <ioj/input/logical/flight_input.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

namespace ioj::tests {
TEST(FlightInput, MultipleDevicesProduceOneOrderedPressAndRelease) {
    ControlsSettings settings{};
    FlightInputState input;
    LogicalInputState state;
    InputDispatcher dispatch;
    reset_flight_input(input, state);
    map_flight_button(input,
                      state,
                      PhysicalKey::LeftMouseButton,
                      ButtonState::Pressed,
                      settings,
                      InputContext::Gameplay);
    map_flight_button(input,
                      state,
                      PhysicalKey::GamepadRightTriggerAxis,
                      ButtonState::Pressed,
                      settings,
                      InputContext::Gameplay);
    map_flight_button(input,
                      state,
                      PhysicalKey::LeftMouseButton,
                      ButtonState::Released,
                      settings,
                      InputContext::Gameplay);
    dispatch.publish_ship(state);
    ASSERT_EQ(dispatch.ship_input().events.size(), 1u);
    EXPECT_EQ(dispatch.ship_input().events.front(), sim::player::PlayerInputKind::FirePressed);

    state.begin_frame();
    map_flight_button(input,
                      state,
                      PhysicalKey::GamepadRightTriggerAxis,
                      ButtonState::Released,
                      settings,
                      InputContext::Gameplay);
    map_flight_button(input,
                      state,
                      PhysicalKey::LeftShift,
                      ButtonState::Pressed,
                      settings,
                      InputContext::Gameplay);
    reset_flight_input(input, state);
    dispatch.publish_ship(state);
    auto const events{dispatch.ship_input().events};
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0], sim::player::PlayerInputKind::FireReleased);
    EXPECT_EQ(events[1], sim::player::PlayerInputKind::BoostPressed);
    EXPECT_EQ(events[2], sim::player::PlayerInputKind::BoostReleased);
}

TEST(FlightInput, EdgesBetweenTicksKeepTheirOrder) {
    ControlsSettings settings{};
    FlightInputState bindings;
    LogicalInputState state;
    InputDispatcher dispatch;
    reset_flight_input(bindings, state);
    for (auto edge : {ButtonState::Pressed, ButtonState::Released, ButtonState::Pressed}) {
        map_flight_button(
            bindings, state, PhysicalKey::LeftMouseButton, edge, settings, InputContext::Gameplay);
    }
    EXPECT_TRUE(state.held.fire());
    using enum sim::player::PlayerInputKind;
    dispatch.publish_ship(state);
    EXPECT_TRUE(std::ranges::equal(dispatch.ship_input().events,
                                   std::array{FirePressed, FireReleased, FirePressed}));
    state.begin_frame();
    EXPECT_TRUE(state.held.fire());
    EXPECT_TRUE(state.transitions.empty());
}

TEST(FlightInput, ReboundPauseAndViewerControlsAreIsolatedFromFlight) {
    ControlsSettings settings{};
    settings.bindings[FlightMapping::GeneralPauseKeyboardMouse] = PhysicalKey::P;
    FlightInputState input;
    LogicalInputState state;
    InputDispatcher dispatch;
    reset_flight_input(input, state);
    map_flight_button(
        input, state, PhysicalKey::Escape, ButtonState::Pressed, settings, InputContext::Observer);
    EXPECT_FALSE(state.held.pause());
    map_flight_button(
        input, state, PhysicalKey::P, ButtonState::Pressed, settings, InputContext::Observer);
    EXPECT_TRUE(state.held.pause());
    map_flight_button(
        input, state, PhysicalKey::P, ButtonState::Pressed, settings, InputContext::Observer);
    EXPECT_EQ(state.transitions.size(), 1u);
    map_flight_button(
        input, state, PhysicalKey::W, ButtonState::Pressed, settings, InputContext::Observer);
    map_flight_button(input,
                      state,
                      PhysicalKey::LeftShift,
                      ButtonState::Pressed,
                      settings,
                      InputContext::Observer);
    dispatch.publish_ship(state);
    EXPECT_TRUE(dispatch.ship_input().events.empty());
    EXPECT_EQ(dispatch.ship_input().accelerator, 0.f);
    settings.bindings[FlightMapping::GeneralPauseKeyboardMouse] = PhysicalKey::NonUSHash;
    reset_flight_input(input, state);
    map_flight_button(
        input, state, PhysicalKey::NonUSHash, ButtonState::Pressed, settings, InputContext::Menu);
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
    reset_flight_input(flight, state);
    map_flight_pointer(flight, state, {10.f, 20.f}, controls);
    EXPECT_TRUE(state.transitions.empty());
    EXPECT_EQ(state.axes_1d[Axis1DAction::Accelerate], 0.f);
    map_flight_button(flight,
                      state,
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
    InputDispatcher dispatch;
    reset_flight_input(input, state);
    map_flight_axis(input, state, PhysicalKey::GamepadLeftX, -1.f, settings);
    map_flight_button(input,
                      state,
                      PhysicalKey::RightMouseButton,
                      ButtonState::Pressed,
                      settings,
                      InputContext::Gameplay);
    map_flight_pointer(input, state, {200.f, 0.f}, settings);
    state.begin_frame();
    dispatch.publish_ship(state);
    EXPECT_FLOAT_EQ(dispatch.ship_input().yaw, 0.5f);
    map_flight_button(input,
                      state,
                      PhysicalKey::RightMouseButton,
                      ButtonState::Released,
                      settings,
                      InputContext::Gameplay);
    dispatch.publish_ship(state);
    EXPECT_FLOAT_EQ(dispatch.ship_input().yaw, -1.f);
    reset_flight_input(input, state);
    dispatch.publish_ship(state);
    EXPECT_EQ(dispatch.ship_input().yaw, 0.f);
    settings.bindings[FlightMapping::FlightYawKeyboardMouse] = PhysicalKey::J;
    reset_flight_input(input, state);
    map_flight_button(
        input, state, PhysicalKey::J, ButtonState::Pressed, settings, InputContext::Gameplay);
    map_flight_button(input,
                      state,
                      PhysicalKey::RightMouseButton,
                      ButtonState::Pressed,
                      settings,
                      InputContext::Gameplay);
    dispatch.publish_ship(state);
    EXPECT_FLOAT_EQ(dispatch.ship_input().yaw, 1.f);
    state.begin_frame();
    dispatch.publish_ship(state);
    EXPECT_FLOAT_EQ(dispatch.ship_input().yaw, 1.f);
    map_flight_button(
        input, state, PhysicalKey::J, ButtonState::Released, settings, InputContext::Gameplay);
    dispatch.publish_ship(state);
    EXPECT_FLOAT_EQ(dispatch.ship_input().yaw, 0.f);
}
}
