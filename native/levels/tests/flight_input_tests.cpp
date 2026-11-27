#include <ioj/input/flight_input.h>

#include <gtest/gtest.h>

namespace ioj::input::tests {
TEST(FlightInput, MultipleDevicesProduceOneOrderedPressAndRelease) {
    ControlsSettings settings{};
    FlightInput input;
    input.configure(settings, Context::Gameplay);
    input.update("LeftMouseButton", 1.f, settings);
    input.update("Gamepad_RightTriggerAxis", 1.f, settings);
    input.update("LeftMouseButton", 0.f, settings);
    ASSERT_EQ(input.frame().events.size(), 1u);
    EXPECT_EQ(input.frame().events.front(), sim::player::PlayerInputKind::FirePressed);

    input.begin_frame();
    input.update("Gamepad_RightTriggerAxis", 0.f, settings);
    input.update("LeftShift", 1.f, settings);
    input.reset();
    auto const events{input.frame().events};
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0], sim::player::PlayerInputKind::FireReleased);
    EXPECT_EQ(events[1], sim::player::PlayerInputKind::BoostPressed);
    EXPECT_EQ(events[2], sim::player::PlayerInputKind::BoostReleased);
}

TEST(FlightInput, ReboundPauseAndViewerControlsAreIsolatedFromFlight) {
    ControlsSettings settings{};
    settings.bindings.emplace().push_back({"general.pause.keyboardmouse", "p"});
    FlightInput input;
    input.configure(settings, Context::BenchmarkViewer);
    EXPECT_FALSE(input.update("Escape", 1.f, settings));
    EXPECT_TRUE(input.update("P", 1.f, settings));
    EXPECT_FALSE(input.update("P", 1.f, settings));
    input.update("W", 1.f, settings);
    input.update("LeftShift", 1.f, settings);
    EXPECT_TRUE(input.frame().events.empty());
    EXPECT_EQ(input.frame().accelerator, 0.f);
}

TEST(FlightInput, PointerSteeringHandsBackToHeldStickAndClearsOnContextChange) {
    ControlsSettings settings{.mouse_turn_sensitivity = 1.f, .gamepad_turn_sensitivity = 1.f};
    FlightInput input;
    input.configure(settings, Context::Gameplay);
    input.update("Gamepad_LeftX", -1.f, settings);
    input.update("RightMouseButton", 1.f, settings);
    input.update("MouseX", 200.f, settings);
    input.begin_frame();
    EXPECT_FLOAT_EQ(input.frame().yaw, 0.5f);
    input.update("RightMouseButton", 0.f, settings);
    EXPECT_FLOAT_EQ(input.frame().yaw, -1.f);
    input.configure(settings, Context::Menu);
    EXPECT_EQ(input.frame().yaw, 0.f);
    settings.bindings.emplace().push_back({"Starfox.Yaw.KeyboardMouse", "J"});
    input.configure(settings, Context::Gameplay);
    input.update("J", 1.f, settings);
    input.update("RightMouseButton", 1.f, settings);
    EXPECT_FLOAT_EQ(input.frame().yaw, 1.f);
    input.begin_frame();
    EXPECT_FLOAT_EQ(input.frame().yaw, 1.f);
    input.update("J", 0.f, settings);
    EXPECT_FLOAT_EQ(input.frame().yaw, 0.f);
}
}
