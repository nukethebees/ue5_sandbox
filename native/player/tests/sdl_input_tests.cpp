#include "player/game_input.h"
#include "player/pause_menu.h"
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
    std::vector<MenuCommand> commands;
    std::vector<KeyCode> keys;
    std::vector<PointerAction> pointer;
    std::string text;
    std::optional<ViewerMotion> motion;
    std::vector<PlayerAction> actions;
};
Presentation presentation;
}

namespace ioj::host {

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
    PauseMenu pause_menu_;
    std::vector<host::PauseCommand> pause_commands_;
    void SetUp() override {
        SDL_SetMainReady();
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) << SDL_GetError();
        host::tests::presentation = {};
        window_ = SDL_CreateWindow("Input test", 100, 100, 0);
        ASSERT_NE(window_, nullptr) << SDL_GetError();
        ASSERT_EQ(SDL_GetKeyboardFocus(), window_);
        devices_.emplace(*window_);
        input_.emplace(settings_, WindowFocus::Gained);
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
            input_->synchronize(commands_,
                                pause_menu_.route(host::tests::presentation.route),
                                host::tests::presentation.menu.input_mode);
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
                if (auto command{pause_menu_.handle(*action, sim::LevelState::Running)}) {
                    pause_commands_.push_back(*command);
                    if (command->kind == host::PauseCommandKind::CheckLeave) {
                        pause_menu_.widget_event(
                            {host::PauseWidgetEventKind::LeaveAccepted, command->item});
                    }
                }
            } else if (auto const* button{std::get_if<host::MenuButton>(&event)}) {
                if (button->down && pause_menu_.view == PauseMenuView::Sidebar) {
                    if (auto command{pause_menu_.navigate(button->command, button->repeat)}) {
                        pause_commands_.push_back(*command);
                    }
                } else if (button->down) {
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
    auto ship_frame() { return ship_input_frame(input_->state(), ship_events_); }
};

TEST_F(SdlPlayerInput, RoutesQueuedInputAcrossMenuFlightViewerAndBenchmark) {
    auto& view{host::tests::presentation};
    press(SDLK_DOWN, SDL_SCANCODE_DOWN);
    frame();
    view.menu.input_mode = MenuInputMode::TextEntry;
    SDL_Event text{};
    text.text.type = SDL_EVENT_TEXT_INPUT;
    text.text.windowID = SDL_GetWindowID(window_);
    text.text.text = "abc";
    ASSERT_TRUE(SDL_PushEvent(&text));
    frame();
    EXPECT_EQ(view.commands, std::vector{MenuCommand::Down});
    EXPECT_EQ(view.text, "abc");
    view.menu.input_mode = MenuInputMode::Navigation;
    press(SDLK_TAB,
          SDL_SCANCODE_TAB,
          SDL_KMOD_RSHIFT | SDL_KMOD_LCTRL | SDL_KMOD_RALT | SDL_KMOD_LGUI | SDL_KMOD_CAPS);
    frame();
    EXPECT_EQ(view.commands.back(), MenuCommand::Previous);
    host::Modifiers expected;
    expected.set_right_shift(true);
    expected.set_left_control(true);
    expected.set_right_alt(true);
    expected.set_left_command(true);
    expected.set_caps_lock(true);
    EXPECT_EQ(view.modifiers, expected);
    input_->resolve(commands_,
                    PhysicalButton{PhysicalKey::Tab, 0, {}, ButtonState::Released});
    press(SDLK_TAB,
          SDL_SCANCODE_TAB,
          SDL_KMOD_LSHIFT | SDL_KMOD_RCTRL | SDL_KMOD_LALT | SDL_KMOD_RGUI);
    frame();
    EXPECT_EQ(view.commands.back(), MenuCommand::Previous);
    expected = {};
    expected.set_left_shift(true);
    expected.set_right_control(true);
    expected.set_left_alt(true);
    expected.set_right_command(true);
    EXPECT_EQ(view.modifiers, expected);

    view.route = {InputContext::Gameplay, false};
    press(SDLK_W, SDL_SCANCODE_W);
    press(SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);
    frame();
    EXPECT_EQ(ship_frame().accelerator, 1.f);
    ASSERT_EQ(ship_frame().events.size(), 1u);
    EXPECT_EQ(ship_frame().events.front(), sim::player::PlayerInputKind::BoostPressed);

    view.route = {InputContext::Observer};
    press(SDLK_D, SDL_SCANCODE_D);
    frame();
    ASSERT_TRUE(view.motion);
    EXPECT_EQ(view.motion->right, 1.f);
    EXPECT_EQ(ship_frame().accelerator, 0.f);
    ASSERT_EQ(ship_frame().events.size(), 1u);
    EXPECT_EQ(ship_frame().events.front(), sim::player::PlayerInputKind::BoostReleased);

    input_->resolve(commands_,
                    PhysicalButton{PhysicalKey::Down, 0, {}, ButtonState::Released});
    view.route = {InputContext::Benchmark};
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

    view.route = {InputContext::Menu};
    input_->resolve(commands_,
                    PhysicalButton{PhysicalKey::Down, 0, {}, ButtonState::Released});
    view.menu.input_mode = MenuInputMode::BindingCapture;
    press(SDLK_DOWN, SDL_SCANCODE_DOWN);
    frame();
    EXPECT_EQ(view.keys, std::vector<KeyCode>{PhysicalKey::Down});
    EXPECT_EQ(view.commands.size(), 3u);
    EXPECT_EQ(move_main_selection(MainMenuItem::SelectLevel, MenuCommand::Up),
              MainMenuItem::Quit);
    EXPECT_EQ(move_main_selection(MainMenuItem::Quit, MenuCommand::Down),
              MainMenuItem::SelectLevel);
}

TEST_F(SdlPlayerInput, MenuCommandsFollowSemanticOwnershipAndRetainEventModifiers) {
    using enum ButtonState;
    host::Modifiers shift{};
    shift.set_left_shift(true);
    auto key = [&](PhysicalKey code,
                   ButtonState edge,
                   host::Modifiers modifiers = {},
                   bool repeat = false) {
        input_->resolve(commands_, PhysicalButton{code, 0, modifiers, edge, repeat});
    };
    key(PhysicalKey::Tab, Pressed, shift);
    key(PhysicalKey::Tab, Pressed, {}, true);
    key(PhysicalKey::Tab, Released);
    key(PhysicalKey::Tab, Pressed, {}, true);
    key(PhysicalKey::GamepadDPadDown, Pressed);
    key(PhysicalKey::Down, Pressed);
    input_->repeat_navigation(commands_, 0.41);
    key(PhysicalKey::GamepadDPadDown, Released);
    input_->repeat_navigation(commands_, 0.41);
    input_->synchronize(commands_, {InputContext::Gameplay}, {});

    ASSERT_EQ(commands_.size(), 6u);
    auto const& transitions{input_->state().transitions};
    ASSERT_EQ(transitions.size(), 4u);
    std::size_t transition_index{};
    for (std::size_t i{}; i < commands_.size(); ++i) {
        auto const& button{std::get<host::MenuButton>(commands_[i])};
        EXPECT_EQ(button.command, i < 3 ? MenuCommand::Previous : MenuCommand::Down);
        EXPECT_EQ(button.down, i != 2 && i != 5);
        EXPECT_EQ(button.repeat, i == 1 || i == 4);
        EXPECT_EQ(button.modifiers.left_shift(), i == 0);
        if (!button.repeat) {
            auto const transition{transitions[transition_index++]};
            EXPECT_EQ(transition.action,
                      i < 3 ? DigitalAction::MenuPrevious : DigitalAction::MenuDown);
            EXPECT_EQ(transition.state, button.down ? Pressed : Released);
        }
    }
}

TEST_F(SdlPlayerInput, PauseReroutesTheNextEventInTheSamePoll) {
    auto& view{host::tests::presentation};
    view.route = {InputContext::Gameplay};
    press(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    press(SDLK_DOWN, SDL_SCANCODE_DOWN);
    press(SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);
    frame();
    EXPECT_EQ(view.actions, std::vector{host::PlayerAction::OpenPause});
    EXPECT_TRUE(view.commands.empty());
    EXPECT_EQ(pause_menu_.selected, host::PauseMenuItem::Controls);
    EXPECT_EQ(pause_commands_,
              (std::vector<host::PauseCommand>{
                  {host::PauseCommandKind::Open, host::PauseMenuItem::Resume},
                  {host::PauseCommandKind::Select, host::PauseMenuItem::Controls}}));
    EXPECT_TRUE(ship_frame().events.empty());
}

TEST_F(SdlPlayerInput, FocusLossClearsHeldActionsAndObserverMotion) {
    auto& view{host::tests::presentation};
    view.route = {InputContext::Observer};
    frame();
    input_->resolve(commands_,
                    PhysicalPointerButton{
                        {}, PhysicalKey::RightMouseButton, ButtonState::Pressed, 1});
    input_->resolve(commands_, PointerMotion{{}, {20.f, 10.f}});
    input_->resolve(commands_, WheelMotion{{}, 1.f});
    input_->resolve(commands_,
                    PhysicalButton{PhysicalKey::D, 0, {}, ButtonState::Pressed});
    EXPECT_TRUE(input_->state().held.observer_right());
    EXPECT_EQ(input_->state().observer_speed, ObserverSpeedPreset::Fast);
    EXPECT_EQ(input_->state().axes_2d[Axis2DAction::ObserverLookDelta],
              (Vector2f{20.f, -10.f}));
    input_->resolve(commands_, WindowFocus::Lost);
    EXPECT_FALSE(input_->relative_mouse());
    EXPECT_FALSE(input_->state().held.observer_right());
    EXPECT_EQ(input_->state().axes_2d[Axis2DAction::ObserverLookDelta], Vector2f{});

    view.route = {InputContext::Menu};
    input_->resolve(commands_, WindowFocus::Gained);
    frame();
    input_->resolve(
        commands_,
        PhysicalButton{PhysicalKey::GamepadDPadDown, 0, {}, ButtonState::Pressed});
    input_->repeat_navigation(commands_, 0.41);
    ASSERT_EQ(commands_.size(), 2u);
    EXPECT_FALSE(std::get<host::MenuButton>(commands_.front()).repeat);
    EXPECT_TRUE(std::get<host::MenuButton>(commands_.back()).repeat);
}

TEST_F(SdlPlayerInput, FocusChangesAndButtonsInOnePollRespectEventOrder) {
    host::tests::presentation.route = {InputContext::Gameplay};
    frame();
    auto focus = [&](SDL_EventType kind) {
        SDL_Event event{};
        event.window.type = kind;
        event.window.windowID = SDL_GetWindowID(window_);
        ASSERT_TRUE(SDL_PushEvent(&event));
    };

    focus(SDL_EVENT_WINDOW_FOCUS_LOST);
    press(SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);
    focus(SDL_EVENT_WINDOW_FOCUS_GAINED);
    press(SDLK_S, SDL_SCANCODE_S);
    frame();
    EXPECT_FALSE(input_->state().held.boost());
    EXPECT_TRUE(input_->state().held.brake());
    ship_frame();
    EXPECT_EQ(ship_events_, std::vector{sim::player::PlayerInputKind::BrakePressed});
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
    view.route = {InputContext::Gameplay, false};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    EXPECT_EQ(
        view.pointer,
        (std::vector<host::PointerAction>{host::PointerAction::Down, host::PointerAction::Up}));

    view.route = {InputContext::Observer};
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
    view.route = {InputContext::Gameplay, false};
    event.button.button = SDL_BUTTON_MIDDLE;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    view.route = {InputContext::Menu, true};
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

    event.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.down = true;
    view.route.resume_available = false;
    ASSERT_TRUE(SDL_PushEvent(&event));
    frame();
    EXPECT_EQ(view.pointer.back(), host::PointerAction::Down);
    EXPECT_EQ(view.actions.size(), 2u);
}

TEST_F(SdlPlayerInput, HeldAxisNeedsANewActivationAcrossContextRebindAndFocus) {
    using enum sim::player::PlayerInputKind;
    auto route = [&](InputContext context) { input_->synchronize(commands_, {context}, {}); };
    auto sample = [&](float value) {
        input_->resolve(commands_,
                        PhysicalAxis{PhysicalKey::GamepadRightTriggerAxis, value});
    };
    route(InputContext::Gameplay);
    sample(0.50f);
    sample(0.45f);
    sample(0.51f);
    route(InputContext::Menu);
    route(InputContext::Gameplay);
    sample(0.9f);
    EXPECT_FALSE(input_->state().held.fire());
    sample(0.40f);
    sample(0.51f);
    input_->refresh_bindings(commands_);
    sample(0.7f);
    EXPECT_FALSE(input_->state().held.fire());
    sample(0.3f);
    sample(0.6f);
    input_->resolve(commands_, WindowFocus::Lost);
    sample(0.7f);
    input_->resolve(commands_, WindowFocus::Gained);
    sample(0.8f);
    EXPECT_FALSE(input_->state().held.fire());
    sample(0.39f);
    sample(0.6f);
    input_->resolve(commands_,
                    PhysicalPointerButton{
                        {}, PhysicalKey::LeftMouseButton, ButtonState::Pressed, 1});
    input_->resolve(commands_, GamepadDisconnected{});
    EXPECT_TRUE(input_->state().held.fire());
    input_->resolve(commands_,
                    PhysicalPointerButton{
                        {}, PhysicalKey::LeftMouseButton, ButtonState::Released, 1});
    ship_frame();
    EXPECT_EQ(ship_events_,
              (std::vector{FirePressed,
                           FireReleased,
                           FirePressed,
                           FireReleased,
                           FirePressed,
                           FireReleased,
                           FirePressed,
                           FireReleased}));
}

TEST_F(SdlPlayerInput, RebindingFireToAnAlreadyHeldStickWaitsForNeutral) {
    input_->synchronize(commands_, {InputContext::Gameplay}, {});
    auto sample = [&](float value) {
        input_->resolve(commands_, PhysicalAxis{PhysicalKey::GamepadRightX, value});
    };
    sample(-0.8f);
    settings_.bindings.emplace().push_back(
        {FlightMapping::FlightFirePrimaryGamepad, PhysicalKey::GamepadRightX});
    input_->refresh_bindings(commands_);
    sample(-0.6f);
    EXPECT_TRUE(input_->state().transitions.empty());
    sample(-0.4f);
    sample(-0.5f);
    EXPECT_TRUE(input_->state().held.fire());
    sample(-0.45f);
    EXPECT_TRUE(input_->state().held.fire());
    sample(-0.4f);
    EXPECT_FALSE(input_->state().held.fire());
}

TEST_F(SdlPlayerInput, UnpluggingVirtualGamepadReleasesFlightButton) {
    auto& view{host::tests::presentation};
    view.route = {InputContext::Gameplay, false};
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
    view.route = {InputContext::Menu, true};
    frame();
    EXPECT_FLOAT_EQ(ship_frame().accelerator, 0.f);
    view.route = {InputContext::Gameplay, false};
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
