#include "player/game_input.h"
#include "player/sdl_input.h"
#include <ioj/input/ship_input.h>

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
    bool route_to_pause_menu{};
    void SetUp() override {
        SDL_SetMainReady();
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) << SDL_GetError();
        host::tests::presentation = {};
        window_ = SDL_CreateWindow("Input test", 100, 100, 0);
        ASSERT_NE(window_, nullptr) << SDL_GetError();
        ASSERT_EQ(SDL_GetKeyboardFocus(), window_);
        devices_.emplace(*window_);
        input_.emplace(settings_);
    }
    void TearDown() override {
        input_.reset();
        devices_.reset();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }
    void frame() {
        input_->begin_frame();
        auto synchronize = [&] {
            input_->synchronize(
                commands_, host::tests::presentation.route, host::tests::presentation.menu);
            devices_->set_window_modes(input_->relative_mouse(), input_->editing_text());
        };
        synchronize();
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (auto physical{devices_->translate(event)}) {
                input_->resolve(commands_, *physical);
                consume();
            }
            synchronize();
        }
        input_->repeat_navigation(commands_, 0.016);
        input_->append_frame_commands(0.016, commands_);
        consume();
    }
    void consume() {
        for (auto const& event : commands_) {
            if (auto const* action{std::get_if<host::PlayerAction>(&event)}) {
                host::tests::presentation.actions.push_back(*action);
                if (route_to_pause_menu && *action == host::PlayerAction::OpenPause) {
                    host::tests::presentation.route = {input::Context::Menu, true};
                }
            } else if (auto const* button{std::get_if<host::MenuButton>(&event)}) {
                if (button->down) {
                    host::tests::presentation.commands.push_back(button->command);
                    host::tests::presentation.modifiers = button->modifiers;
                }
            } else {
                host::present_input(std::get<host::PresentationEvent>(event));
            }
        }
        commands_.clear();
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
    std::optional<SdlInput> devices_;
    std::optional<GameInput> input_;
    std::vector<host::InputEvent> commands_;
    std::vector<sim::player::PlayerInputKind> ship_events_;
    auto ship_frame() { return input::ship_input_frame(input_->state(), ship_events_); }
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
    input_->resolve(commands_,
                    input::PhysicalButton{PhysicalKey::Tab, 0, {}, input::ButtonState::Released});
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
    EXPECT_EQ(ship_frame().accelerator, 1.f);
    ASSERT_EQ(ship_frame().events.size(), 1u);
    EXPECT_EQ(ship_frame().events.front(), sim::player::PlayerInputKind::BoostPressed);

    view.route = {input::Context::Observer};
    press(SDLK_D, SDL_SCANCODE_D);
    frame();
    ASSERT_TRUE(view.motion);
    EXPECT_EQ(view.motion->right, 1.f);
    EXPECT_EQ(ship_frame().accelerator, 0.f);
    ASSERT_EQ(ship_frame().events.size(), 1u);
    EXPECT_EQ(ship_frame().events.front(), sim::player::PlayerInputKind::BoostReleased);

    view.route = {input::Context::Benchmark};
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

TEST_F(SdlPlayerInput, MenuCommandsFollowSemanticOwnershipAndRetainEventModifiers) {
    using enum input::ButtonState;
    host::Modifiers shift{};
    shift.set_left_shift(true);
    auto key = [&](PhysicalKey code, input::ButtonState edge, host::Modifiers modifiers = {}) {
        input_->resolve(commands_, input::PhysicalButton{code, 0, modifiers, edge});
    };
    key(PhysicalKey::Tab, Pressed, shift);
    key(PhysicalKey::Tab, Repeated);
    key(PhysicalKey::Tab, Released);
    key(PhysicalKey::Tab, Repeated);
    key(PhysicalKey::GamepadDPadDown, Pressed);
    key(PhysicalKey::Down, Pressed);
    input_->repeat_navigation(commands_, 0.41);
    key(PhysicalKey::GamepadDPadDown, Released);
    input_->repeat_navigation(commands_, 0.41);
    input_->synchronize(commands_, {input::Context::Gameplay}, {});

    ASSERT_EQ(commands_.size(), 6u);
    auto const& transitions{input_->state().transitions};
    ASSERT_EQ(transitions.size(), commands_.size());
    for (std::size_t i{}; i < commands_.size(); ++i) {
        auto const& button{std::get<host::MenuButton>(commands_[i])};
        EXPECT_EQ(button.command, i < 3 ? input::MenuCommand::Previous : input::MenuCommand::Down);
        EXPECT_EQ(button.down, i != 2 && i != 5);
        EXPECT_EQ(button.repeat, i == 1 || i == 4);
        EXPECT_EQ(button.modifiers.left_shift(), i == 0);
        EXPECT_EQ(transitions[i].action,
                  i < 3 ? input::DigitalAction::MenuPrevious : input::DigitalAction::MenuDown);
        EXPECT_EQ(transitions[i].state,
                  button.repeat ? Repeated
                  : button.down ? Pressed
                                : Released);
    }
}

TEST_F(SdlPlayerInput, PauseReroutesTheNextEventInTheSamePoll) {
    auto& view{host::tests::presentation};
    route_to_pause_menu = true;
    view.route = {input::Context::Gameplay};
    press(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    press(SDLK_DOWN, SDL_SCANCODE_DOWN);
    frame();
    EXPECT_EQ(view.actions, std::vector{host::PlayerAction::OpenPause});
    EXPECT_EQ(view.commands, std::vector{input::MenuCommand::Down});
}

TEST_F(SdlPlayerInput, FocusLossClearsHeldActionsAndObserverMotion) {
    auto& view{host::tests::presentation};
    view.route = {input::Context::Observer};
    frame();
    input_->resolve(commands_,
                    input::PhysicalPointerButton{
                        {}, PhysicalKey::RightMouseButton, input::ButtonState::Pressed, 1});
    input_->resolve(commands_, input::PointerMotion{{}, {20.f, 10.f}});
    input_->resolve(commands_, input::WheelMotion{{}, 1.f});
    input_->resolve(commands_,
                    input::PhysicalButton{PhysicalKey::D, 0, {}, input::ButtonState::Pressed});
    EXPECT_TRUE(input_->state().held.observer_right());
    EXPECT_EQ(input_->state().observer_speed, input::ObserverSpeedPreset::Fast);
    EXPECT_EQ(input_->state().axes_2d[input::Axis2DAction::ObserverLookDelta],
              (Vector2f{20.f, -10.f}));
    input_->resolve(commands_, input::FocusLost{});
    EXPECT_FALSE(input_->relative_mouse());
    EXPECT_TRUE(input_->state().released.observer_right());
    EXPECT_EQ(input_->state().axes_2d[input::Axis2DAction::ObserverLookDelta], Vector2f{});

    view.route = {input::Context::Menu};
    frame();
    input_->resolve(
        commands_,
        input::PhysicalButton{PhysicalKey::GamepadDPadDown, 0, {}, input::ButtonState::Pressed});
    input_->repeat_navigation(commands_, 0.41);
    ASSERT_EQ(commands_.size(), 2u);
    EXPECT_FALSE(std::get<host::MenuButton>(commands_.front()).repeat);
    EXPECT_TRUE(std::get<host::MenuButton>(commands_.back()).repeat);
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

    view.route = {input::Context::Observer};
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
    view.route = {input::Context::Menu, true};
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
    ASSERT_EQ(ship_frame().events.size(), 1u);
    EXPECT_EQ(ship_frame().events.front(), sim::player::PlayerInputKind::BoostPressed);
    ASSERT_TRUE(SDL_SetJoystickVirtualAxis(stick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767));
    SDL_UpdateJoysticks();
    frame();
    EXPECT_FLOAT_EQ(ship_frame().accelerator, 1.f);
    view.route = {input::Context::Menu, true};
    frame();
    EXPECT_FLOAT_EQ(ship_frame().accelerator, 0.f);
    view.route = {input::Context::Gameplay, false};
    frame();
    EXPECT_FLOAT_EQ(ship_frame().accelerator, 1.f);
    EXPECT_TRUE(ship_frame().events.empty());
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, false));
    SDL_UpdateJoysticks();
    frame();
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true));
    SDL_UpdateJoysticks();
    frame();
    ASSERT_TRUE(SDL_DetachVirtualJoystick(id));
    frame();
    ASSERT_EQ(ship_frame().events.size(), 1u);
    EXPECT_EQ(ship_frame().events.front(), sim::player::PlayerInputKind::BoostReleased);
}
}
