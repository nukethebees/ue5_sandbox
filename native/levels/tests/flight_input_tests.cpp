#include <ioj/input/flight_input.h>
#include <ioj/input/ship_input.h>

#include <gtest/gtest.h>

namespace ioj::input::tests {
TEST(FlightInput, MultipleDevicesProduceOneOrderedPressAndRelease) {
    ControlsSettings settings{};
    FlightInputResolver input;
    InputState state;
    std::vector<sim::player::PlayerInputKind> ship_events;
    input.configure(state, settings, Context::Gameplay);
    input.button(state, PhysicalKey::LeftMouseButton, ButtonState::Pressed, settings);
    input.button(state, PhysicalKey::GamepadRightTriggerAxis, ButtonState::Pressed, settings);
    input.button(state, PhysicalKey::LeftMouseButton, ButtonState::Released, settings);
    ASSERT_EQ(ship_input_frame(state, ship_events).events.size(), 1u);
    EXPECT_EQ(ship_input_frame(state, ship_events).events.front(),
              sim::player::PlayerInputKind::FirePressed);

    state.begin_frame();
    input.button(state, PhysicalKey::GamepadRightTriggerAxis, ButtonState::Released, settings);
    input.button(state, PhysicalKey::LeftShift, ButtonState::Pressed, settings);
    input.reset(state);
    auto const events{ship_input_frame(state, ship_events).events};
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0], sim::player::PlayerInputKind::FireReleased);
    EXPECT_EQ(events[1], sim::player::PlayerInputKind::BoostPressed);
    EXPECT_EQ(events[2], sim::player::PlayerInputKind::BoostReleased);
}

TEST(FlightInput, RepeatedEdgesBetweenTicksKeepTheirOrderAndFrameFlags) {
    ControlsSettings settings{};
    FlightInputResolver bindings;
    InputState state;
    std::vector<sim::player::PlayerInputKind> events;
    bindings.configure(state, settings, Context::Gameplay);
    for (auto edge : {ButtonState::Pressed, ButtonState::Released, ButtonState::Pressed}) {
        bindings.button(state, PhysicalKey::LeftMouseButton, edge, settings);
    }
    EXPECT_TRUE(state.held.fire());
    EXPECT_TRUE(state.pressed.fire());
    EXPECT_TRUE(state.released.fire());
    using enum sim::player::PlayerInputKind;
    ship_input_frame(state, events);
    EXPECT_EQ(events, (std::vector{FirePressed, FireReleased, FirePressed}));
    state.begin_frame();
    EXPECT_TRUE(state.held.fire());
    EXPECT_FALSE(state.pressed.fire());
    EXPECT_FALSE(state.released.fire());
    EXPECT_TRUE(state.transitions.empty());
}

TEST(FlightInput, ReboundPauseAndViewerControlsAreIsolatedFromFlight) {
    ControlsSettings settings{};
    settings.bindings.emplace().push_back(
        {*parse_mapping("general.pause.keyboardmouse"), parse_key("p")});
    FlightInputResolver input;
    InputState state;
    std::vector<sim::player::PlayerInputKind> ship_events;
    input.configure(state, settings, Context::Observer);
    input.button(state, PhysicalKey::Escape, ButtonState::Pressed, settings);
    EXPECT_FALSE(state.held.pause());
    input.button(state, PhysicalKey::P, ButtonState::Pressed, settings);
    EXPECT_TRUE(state.held.pause());
    input.button(state, PhysicalKey::P, ButtonState::Pressed, settings);
    EXPECT_EQ(state.transitions.size(), 1u);
    input.button(state, PhysicalKey::W, ButtonState::Pressed, settings);
    input.button(state, PhysicalKey::LeftShift, ButtonState::Pressed, settings);
    EXPECT_TRUE(ship_input_frame(state, ship_events).events.empty());
    EXPECT_EQ(ship_input_frame(state, ship_events).accelerator, 0.f);
    settings.bindings->front().key = parse_key("#");
    input.configure(state, settings, Context::Menu);
    input.button(state, character_key(35), ButtonState::Pressed, settings);
    EXPECT_TRUE(state.held.pause());
    for (auto key : {"MouseScrollUp", "MouseX"}) {
        settings.bindings->front().key = parse_key(key);
        input.configure(state, settings, Context::Menu);
        EXPECT_FALSE(input.is_pause_button(parse_key(key)));
        EXPECT_TRUE(input.is_pause_button(PhysicalKey::Escape));
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
    controls.bindings.emplace().push_back({FlightFirePrimaryKeyboardMouse, PhysicalKey::MouseX});
    controls.bindings->push_back({FlightAccelerateKeyboardMouse, PhysicalKey::MouseY});
    FlightInputResolver flight;
    InputState state;
    flight.configure(state, controls, Context::Gameplay);
    flight.pointer_motion(state, {10.f, 20.f}, controls);
    EXPECT_TRUE(state.transitions.empty());
    EXPECT_EQ(state.axes_1d[Axis1DAction::Accelerate], 0.f);
    flight.button(state, PhysicalKey::LeftMouseButton, ButtonState::Pressed, controls);
    EXPECT_TRUE(state.held.fire());
}

TEST(FlightInput, PointerSteeringHandsBackToHeldStickAndClearsOnContextChange) {
    ControlsSettings settings{.mouse_turn_sensitivity = 1.f, .gamepad_turn_sensitivity = 1.f};
    FlightInputResolver input;
    InputState state;
    std::vector<sim::player::PlayerInputKind> ship_events;
    input.configure(state, settings, Context::Gameplay);
    input.sample_axis(state, PhysicalKey::GamepadLeftX, -1.f, settings);
    input.button(state, PhysicalKey::RightMouseButton, ButtonState::Pressed, settings);
    input.pointer_motion(state, {200.f, 0.f}, settings);
    state.begin_frame();
    EXPECT_FLOAT_EQ(ship_input_frame(state, ship_events).yaw, 0.5f);
    input.button(state, PhysicalKey::RightMouseButton, ButtonState::Released, settings);
    EXPECT_FLOAT_EQ(ship_input_frame(state, ship_events).yaw, -1.f);
    input.configure(state, settings, Context::Menu);
    EXPECT_EQ(ship_input_frame(state, ship_events).yaw, 0.f);
    settings.bindings.emplace().push_back({FlightMapping::FlightYawKeyboardMouse, PhysicalKey::J});
    input.configure(state, settings, Context::Gameplay);
    input.button(state, PhysicalKey::J, ButtonState::Pressed, settings);
    input.button(state, PhysicalKey::RightMouseButton, ButtonState::Pressed, settings);
    EXPECT_FLOAT_EQ(ship_input_frame(state, ship_events).yaw, 1.f);
    state.begin_frame();
    EXPECT_FLOAT_EQ(ship_input_frame(state, ship_events).yaw, 1.f);
    input.button(state, PhysicalKey::J, ButtonState::Released, settings);
    EXPECT_FLOAT_EQ(ship_input_frame(state, ship_events).yaw, 0.f);
}
}
