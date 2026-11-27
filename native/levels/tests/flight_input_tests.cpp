#include <ioj/input/flight_input.h>

#include <gtest/gtest.h>

namespace ioj::input::tests {
TEST(FlightInput, MultipleDevicesProduceOneOrderedPressAndRelease) {
    ControlsSettings settings{};
    FlightInput input;
    input.configure(settings, Context::Gameplay);
    input.update(PhysicalKey::LeftMouseButton, 1.f, settings);
    input.update(PhysicalKey::GamepadRightTriggerAxis, 1.f, settings);
    input.update(PhysicalKey::LeftMouseButton, 0.f, settings);
    ASSERT_EQ(input.frame().events.size(), 1u);
    EXPECT_EQ(input.frame().events.front(), sim::player::PlayerInputKind::FirePressed);

    input.begin_frame();
    input.update(PhysicalKey::GamepadRightTriggerAxis, 0.f, settings);
    input.update(PhysicalKey::LeftShift, 1.f, settings);
    input.reset();
    auto const events{input.frame().events};
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0], sim::player::PlayerInputKind::FireReleased);
    EXPECT_EQ(events[1], sim::player::PlayerInputKind::BoostPressed);
    EXPECT_EQ(events[2], sim::player::PlayerInputKind::BoostReleased);
}

TEST(FlightInput, ReboundPauseAndViewerControlsAreIsolatedFromFlight) {
    ControlsSettings settings{};
    settings.bindings.emplace().push_back(
        {*parse_mapping("general.pause.keyboardmouse"), parse_key("p")});
    FlightInput input;
    input.configure(settings, Context::BenchmarkViewer);
    EXPECT_FALSE(input.update(PhysicalKey::Escape, 1.f, settings));
    EXPECT_TRUE(input.update(PhysicalKey::P, 1.f, settings));
    EXPECT_FALSE(input.update(PhysicalKey::P, 1.f, settings));
    input.update(PhysicalKey::W, 1.f, settings);
    input.update(PhysicalKey::LeftShift, 1.f, settings);
    EXPECT_TRUE(input.frame().events.empty());
    EXPECT_EQ(input.frame().accelerator, 0.f);
    settings.bindings->front().key = parse_key("#");
    input.configure(settings, Context::Menu);
    EXPECT_TRUE(input.update(character_key(35), 1.f, settings));
    for (auto key : {"MouseScrollUp", "MouseX"}) {
        settings.bindings->front().key = parse_key(key);
        input.configure(settings, Context::Menu);
        EXPECT_FALSE(input.is_pause_button(parse_key(key)));
        EXPECT_TRUE(input.is_pause_button(PhysicalKey::Escape));
    }
}

TEST(FlightInput, PointerSteeringHandsBackToHeldStickAndClearsOnContextChange) {
    ControlsSettings settings{.mouse_turn_sensitivity = 1.f, .gamepad_turn_sensitivity = 1.f};
    FlightInput input;
    input.configure(settings, Context::Gameplay);
    input.update(PhysicalKey::GamepadLeftX, -1.f, settings);
    input.update(PhysicalKey::RightMouseButton, 1.f, settings);
    input.update(PhysicalKey::MouseX, 200.f, settings);
    input.begin_frame();
    EXPECT_FLOAT_EQ(input.frame().yaw, 0.5f);
    input.update(PhysicalKey::RightMouseButton, 0.f, settings);
    EXPECT_FLOAT_EQ(input.frame().yaw, -1.f);
    input.configure(settings, Context::Menu);
    EXPECT_EQ(input.frame().yaw, 0.f);
    settings.bindings.emplace().push_back({FlightMapping::StarfoxYawKeyboardMouse, PhysicalKey::J});
    input.configure(settings, Context::Gameplay);
    input.update(PhysicalKey::J, 1.f, settings);
    input.update(PhysicalKey::RightMouseButton, 1.f, settings);
    EXPECT_FLOAT_EQ(input.frame().yaw, 1.f);
    input.begin_frame();
    EXPECT_FLOAT_EQ(input.frame().yaw, 1.f);
    input.update(PhysicalKey::J, 0.f, settings);
    EXPECT_FLOAT_EQ(input.frame().yaw, 0.f);
}
}
