#include "player/sdl_input.h"

#include <gtest/gtest.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

#include <optional>
#include <string>
#include <vector>

namespace ioj::host::tests {
static_assert(sizeof(Modifiers) == 2);
struct Presentation {
    InputRoute route;
    MenuStatus menu;
    Modifiers modifiers;
    std::vector<input::MenuCommand> commands;
    std::vector<input::KeyCode> keys;
    std::vector<PointerAction> pointer;
    std::string text;
    std::optional<ViewerMotion> motion;
    std::vector<PlayerAction> actions;
};
Presentation presentation;
}

namespace ioj::host {

void read_legacy_key_bindings(void*, LegacyBindingSink) {}
void present_input(PresentationEvent const& event) {
    auto& view{tests::presentation};
    if (auto const* key{std::get_if<MenuKey>(&event)}; key && key->down) {
        view.keys.push_back(key->key);
    }
    if (auto const* text{std::get_if<MenuText>(&event)}) {
        view.text += text->text;
    }
    if (auto const* pointer{std::get_if<MenuPointer>(&event)}) {
        view.pointer.push_back(pointer->pointer.action);
    }
    if (auto const* step{std::get_if<ViewerStep>(&event)}) {
        view.motion = step->motion;
    }
}

}

namespace ioj::tests {
class SdlPlayerInput : public ::testing::Test {
  protected:
    void SetUp() override {
        SDL_SetMainReady();
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) << SDL_GetError();
        host::tests::presentation = {};
        window_ = SDL_CreateWindow("Input test", 100, 100, 0);
        ASSERT_NE(window_, nullptr) << SDL_GetError();
        ASSERT_EQ(SDL_GetKeyboardFocus(), window_);
        input_.emplace(*window_, settings_);
    }
    void TearDown() override {
        input_.reset();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }
    void frame() {
        input_->begin_frame();
        input_->synchronize(host::tests::presentation.route, host::tests::presentation.menu);
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            input_->handle(event);
            consume();
        }
        input_->finish_frame(0.016);
        consume();
    }
    void consume() {
        for (auto const& event : input_->events()) {
            if (auto const* action{std::get_if<host::PlayerAction>(&event)}) {
                host::tests::presentation.actions.push_back(*action);
            } else if (auto const* button{std::get_if<host::MenuButton>(&event)}) {
                if (button->down) {
                    host::tests::presentation.commands.push_back(button->command);
                    host::tests::presentation.modifiers = button->modifiers;
                }
            } else {
                host::present_input(std::get<host::PresentationEvent>(event));
            }
        }
    }
    void press(SDL_Keycode key, SDL_Scancode scancode, SDL_Keymod modifiers = SDL_KMOD_NONE) {
        SDL_Event event{};
        event.key.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = SDL_GetWindowID(window_);
        event.key.scancode = scancode;
        event.key.key = key;
        event.key.down = true;
        event.key.mod = modifiers;
        ASSERT_TRUE(SDL_PushEvent(&event));
    }
    ControlsSettings settings_{};
    SDL_Window* window_{};
    std::optional<SdlInput> input_;
};

TEST_F(SdlPlayerInput, RoutesQueuedInputAcrossMenuFlightViewerAndBenchmark) {
    auto& view{host::tests::presentation};
    press(SDLK_DOWN, SDL_SCANCODE_DOWN);
    frame();
    view.menu.editing_text = true;
    SDL_Event text{};
    text.text.type = SDL_EVENT_TEXT_INPUT;
    text.text.windowID = SDL_GetWindowID(window_);
    text.text.text = "abc";
    ASSERT_TRUE(SDL_PushEvent(&text));
    frame();
    EXPECT_EQ(view.commands, std::vector{input::MenuCommand::Down});
    EXPECT_EQ(view.text, "abc");
    view.menu.editing_text = false;
    press(SDLK_TAB,
          SDL_SCANCODE_TAB,
          SDL_KMOD_RSHIFT | SDL_KMOD_LCTRL | SDL_KMOD_RALT | SDL_KMOD_LGUI | SDL_KMOD_CAPS);
    frame();
    EXPECT_EQ(view.commands.back(), input::MenuCommand::Previous);
    host::Modifiers expected;
    expected.set_right_shift(true);
    expected.set_left_control(true);
    expected.set_right_alt(true);
    expected.set_left_command(true);
    expected.set_caps_lock(true);
    EXPECT_EQ(view.modifiers, expected);
    press(SDLK_TAB,
          SDL_SCANCODE_TAB,
          SDL_KMOD_LSHIFT | SDL_KMOD_RCTRL | SDL_KMOD_LALT | SDL_KMOD_RGUI);
    frame();
    EXPECT_EQ(view.commands.back(), input::MenuCommand::Previous);
    expected = {};
    expected.set_left_shift(true);
    expected.set_right_control(true);
    expected.set_left_alt(true);
    expected.set_right_command(true);
    EXPECT_EQ(view.modifiers, expected);

    view.route = {input::Context::Gameplay, false};
    press(SDLK_W, SDL_SCANCODE_W);
    press(SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);
    frame();
    EXPECT_EQ(input_->ship_frame().accelerator, 1.f);
    ASSERT_EQ(input_->ship_frame().events.size(), 1u);
    EXPECT_EQ(input_->ship_frame().events.front(), sim::player::PlayerInputKind::BoostPressed);

    view.route = {input::Context::Observer, true};
    press(SDLK_D, SDL_SCANCODE_D);
    frame();
    ASSERT_TRUE(view.motion);
    EXPECT_EQ(view.motion->right, 1.f);
    EXPECT_EQ(input_->ship_frame().accelerator, 0.f);
    ASSERT_EQ(input_->ship_frame().events.size(), 1u);
    EXPECT_EQ(input_->ship_frame().events.front(), sim::player::PlayerInputKind::BoostReleased);

    view.route = {input::Context::Benchmark, false};
    view.motion.reset();
    press(SDLK_DOWN, SDL_SCANCODE_DOWN);
    press(SDLK_RETURN, SDL_SCANCODE_RETURN);
    press(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    frame();
    EXPECT_EQ(view.actions,
              (std::vector{host::PlayerAction::BenchmarkNext,
                           host::PlayerAction::BenchmarkActivate,
                           host::PlayerAction::EndBenchmark}));
    EXPECT_FALSE(view.motion);
    EXPECT_TRUE(view.keys.empty());

    view.route = {input::Context::Menu};
    view.menu.capturing_binding = true;
    press(SDLK_DOWN, SDL_SCANCODE_DOWN);
    frame();
    EXPECT_EQ(view.keys, std::vector<input::KeyCode>{PhysicalKey::Down});
    EXPECT_EQ(view.commands.size(), 3u);
    EXPECT_EQ(input::move_main_selection(input::MainMenuItem::SelectLevel, input::MenuCommand::Up),
              input::MainMenuItem::Quit);
    EXPECT_EQ(input::move_main_selection(input::MainMenuItem::Quit, input::MenuCommand::Down),
              input::MainMenuItem::SelectLevel);
}

TEST_F(SdlPlayerInput, PointerReleaseFollowsMenuPressAcrossContextChange) {
    auto& view{host::tests::presentation};
    SDL_Event event{};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(window_);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    view.route = {input::Context::Gameplay, false};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    EXPECT_EQ(
        view.pointer,
        (std::vector<host::PointerAction>{host::PointerAction::Down, host::PointerAction::Up}));

    view.route = {input::Context::Observer, true};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = true;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    ASSERT_TRUE(view.motion);
    EXPECT_TRUE(view.motion->looking);
    EXPECT_EQ(view.pointer.size(), 2u);

    settings_.bindings.emplace().push_back(
        {FlightMapping::GeneralPauseKeyboardMouse, PhysicalKey::MiddleMouseButton});
    view.route = {input::Context::Gameplay, false};
    event.button.button = SDL_BUTTON_MIDDLE;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    view.route = {input::Context::Menu, false, true};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.down = true;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    EXPECT_EQ(view.actions,
              (std::vector{host::PlayerAction::OpenPause, host::PlayerAction::RequestResume}));
    EXPECT_EQ(view.pointer.size(), 2u);

    view.route.resume_available = false;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    EXPECT_EQ(view.pointer.back(), host::PointerAction::Down);
    EXPECT_EQ(view.actions.size(), 2u);
}

TEST_F(SdlPlayerInput, UnpluggingVirtualGamepadReleasesFlightButton) {
    auto& view{host::tests::presentation};
    view.route = {input::Context::Gameplay, false};
    SDL_VirtualJoystickDesc device{};
    SDL_INIT_INTERFACE(&device);
    device.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    device.vendor_id = 0xf00d;
    device.product_id = 0x1234;
    device.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    device.naxes = SDL_GAMEPAD_AXIS_COUNT;
    auto const id{SDL_AttachVirtualJoystick(&device)};
    ASSERT_NE(id, 0u) << SDL_GetError();
    frame();
    auto* const gamepad{SDL_GetGamepadFromID(id)};
    ASSERT_NE(gamepad, nullptr) << SDL_GetError();
    auto* const stick{SDL_GetGamepadJoystick(gamepad)};
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true));
    SDL_UpdateJoysticks();
    frame();
    ASSERT_EQ(input_->ship_frame().events.size(), 1u);
    EXPECT_EQ(input_->ship_frame().events.front(), sim::player::PlayerInputKind::BoostPressed);
    ASSERT_TRUE(SDL_SetJoystickVirtualAxis(stick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767));
    SDL_UpdateJoysticks();
    frame();
    EXPECT_FLOAT_EQ(input_->ship_frame().accelerator, 1.f);
    view.route = {input::Context::Menu, false, true};
    frame();
    EXPECT_FLOAT_EQ(input_->ship_frame().accelerator, 0.f);
    view.route = {input::Context::Gameplay, false};
    frame();
    EXPECT_FLOAT_EQ(input_->ship_frame().accelerator, 1.f);
    EXPECT_TRUE(input_->ship_frame().events.empty());
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, false));
    SDL_UpdateJoysticks();
    frame();
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true));
    SDL_UpdateJoysticks();
    frame();
    ASSERT_TRUE(SDL_DetachVirtualJoystick(id));
    frame();
    ASSERT_EQ(input_->ship_frame().events.size(), 1u);
    EXPECT_EQ(input_->ship_frame().events.front(), sim::player::PlayerInputKind::BoostReleased);
}
}
