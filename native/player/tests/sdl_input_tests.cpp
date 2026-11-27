#include "player/input/input_system.h"
#include "player/pause_menu.h"
#include <ioj/host/input.h>
#include <ioj/input/dispatch/player_menu.h>

#include <gtest/gtest.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ioj::host::tests {
static_assert(sizeof(Modifiers) == 2);
static_assert(sizeof(PhysicalKey) == sizeof(std::uint16_t));
static_assert(std::to_underlying(PhysicalKey::A) == SDL_SCANCODE_A);
static_assert(std::to_underlying(PhysicalKey::RightAlt) == SDL_SCANCODE_RALT);
static_assert(!is_keyboard_key(static_cast<PhysicalKey>(SDL_SCANCODE_F24)));
static_assert(!is_keyboard_key(PhysicalKey::LeftMouseButton));
static_assert(!is_keyboard_key(PhysicalKey::GamepadLeftX));
struct Presentation {
    KeyboardLayout layout;
    InputRoute route;
    MenuStatus menu;
    Modifiers modifiers;
    std::vector<MenuCommand> commands;
    std::vector<PhysicalKey> keys;
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
class SdlPlayerInput
    : public ::testing::Test
    , public InputReceiver {
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
        input_.emplace(*window_, settings_, host::tests::presentation.layout);
    }
    void TearDown() override {
        input_.reset();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }
    void frame() { input_->advance(0.016, *this); }
    auto routing() const -> InputRouting override {
        return {pause_menu_.route(host::tests::presentation.route),
                host::tests::presentation.menu.input_mode};
    }
    void consume(std::span<host::MenuEvent const> events) override {
        for (auto const& event : events) {
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
            }
        }
    }
    void consume(std::span<host::PresentationEvent const> events) override {
        for (auto const& event : events) {
            host::present_input(event);
        }
    }
    void key_event(SDL_Keycode key,
                   SDL_Scancode scancode,
                   SDL_Keymod modifiers = SDL_KMOD_NONE,
                   bool down = true) {
        SDL_Event event{};
        event.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.windowID = SDL_GetWindowID(window_);
        event.key.scancode = scancode;
        event.key.key = key;
        event.key.down = down;
        event.key.mod = modifiers;
        ASSERT_TRUE(SDL_PushEvent(&event));
    }
    ControlsSettings settings_{};
    SDL_Window* window_{};
    std::optional<InputSystem> input_;
    auto ship_frame() const { return input_->ship_input(); }
};

TEST_F(SdlPlayerInput, RoutesQueuedInputAcrossMenuFlightViewerAndBenchmark) {
    auto& view{host::tests::presentation};
    EXPECT_EQ(view.layout.label(PhysicalKey::W),
              SDL_GetKeyName(SDL_GetKeyFromScancode(SDL_SCANCODE_W, SDL_KMOD_NONE, false)));
    key_event(SDLK_DOWN, SDL_SCANCODE_DOWN);
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
    key_event(SDLK_TAB,
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
    key_event(SDLK_TAB, SDL_SCANCODE_TAB, SDL_KMOD_NONE, false);
    key_event(SDLK_TAB,
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
    key_event(SDLK_Z, SDL_SCANCODE_W);
    key_event(SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);
    frame();
    EXPECT_EQ(ship_frame().accelerator, 1.f);
    ASSERT_EQ(ship_frame().events.size(), 1u);
    EXPECT_EQ(ship_frame().events.front(), sim::player::PlayerInputKind::BoostPressed);

    view.route = {InputContext::Observer};
    key_event(SDLK_D, SDL_SCANCODE_D);
    frame();
    ASSERT_TRUE(view.motion);
    EXPECT_EQ(view.motion->right, 1.f);
    EXPECT_EQ(ship_frame().accelerator, 0.f);
    ASSERT_EQ(ship_frame().events.size(), 1u);
    EXPECT_EQ(ship_frame().events.front(), sim::player::PlayerInputKind::BoostReleased);

    key_event(SDLK_DOWN, SDL_SCANCODE_DOWN, SDL_KMOD_NONE, false);
    view.route = {InputContext::Benchmark};
    view.motion.reset();
    key_event(SDLK_DOWN, SDL_SCANCODE_DOWN);
    key_event(SDLK_RETURN, SDL_SCANCODE_RETURN);
    key_event(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    frame();
    EXPECT_EQ(view.actions,
              (std::vector{host::PlayerAction::BenchmarkNext,
                           host::PlayerAction::BenchmarkActivate,
                           host::PlayerAction::EndBenchmark}));
    EXPECT_FALSE(view.motion);
    EXPECT_TRUE(view.keys.empty());

    view.route = {InputContext::Menu};
    key_event(SDLK_DOWN, SDL_SCANCODE_DOWN, SDL_KMOD_NONE, false);
    view.menu.input_mode = MenuInputMode::BindingCapture;
    key_event(SDLK_DOWN, SDL_SCANCODE_DOWN);
    key_event(SDLK_A, SDL_SCANCODE_UNKNOWN);
    key_event(SDLK_RETURN, SDL_SCANCODE_KP_ENTER);
    frame();
    EXPECT_EQ(view.keys, (std::vector<PhysicalKey>{PhysicalKey::Down, PhysicalKey::NumPadEnter}));
    EXPECT_EQ(view.commands.size(), 3u);
    PlayerMenu menu;
    PlayerSettingsView settings{};
    ASSERT_TRUE(menu.navigate(MenuCommand::Up, false, settings, {}));
    EXPECT_EQ(menu.focus, MenuTarget{MainMenuItem::Quit});
    ASSERT_TRUE(menu.navigate(MenuCommand::Down, false, settings, {}));
    EXPECT_EQ(menu.focus, MenuTarget{MainMenuItem::SelectLevel});

    using Category = levels::authoring::LevelCatalogCategory;
    menu.levels = {{"battle", Category::BattleViewer, true}};
    ASSERT_TRUE(menu.activate(Category::Benchmark, settings, {}));
    ASSERT_TRUE(menu.navigate(MenuCommand::Down, false, settings, {}));
    EXPECT_EQ(menu.focus, (MenuTarget{MenuRow{MenuRow::Kind::Level, "battle"}}));
    ASSERT_TRUE(menu.navigate(MenuCommand::Up, false, settings, {}));
    EXPECT_EQ(menu.focus, MenuTarget{Category::Benchmark});

    ASSERT_TRUE(menu.activate(MainMenuItem::Audio, settings, {}));
    ASSERT_TRUE(menu.draft.set_setting(GameSetting::MasterVolume, 0.4f));
    ASSERT_TRUE(menu.activate(MainMenuItem::Controls, settings, {}));
    EXPECT_EQ(menu.page, MainMenuItem::Audio);
    EXPECT_EQ(menu.prompt, OptionsPrompt::Dirty);
    EXPECT_EQ(menu.focus, MenuTarget{MenuControl::Save});
    menu.set_prompt(OptionsPrompt::None);
    EXPECT_FALSE(menu.finish_main_leave(false));
    EXPECT_EQ(menu.focus, MenuTarget{MainMenuItem::Audio});
}

class LogicalPlayerInput : public ::testing::Test {
  protected:
    void map(auto event) {
        batch_.clear();
        batch_.push(event);
        input_.map_until_dispatch(dispatch_, batch_, 0);
    }
    ControlsSettings settings_;
    PhysicalInputState physical_;
    PhysicalInputBatch batch_;
    LogicalInputSystem input_{settings_, physical_};
    InputDispatcher dispatch_;
};

TEST_F(LogicalPlayerInput, MenuCommandsFollowSemanticOwnershipAndRetainEventModifiers) {
    using enum ButtonState;
    host::Modifiers shift{};
    shift.set_left_shift(true);
    auto key = [&](PhysicalKey code,
                   ButtonState edge,
                   host::Modifiers modifiers = {},
                   bool repeat = false) { map(PhysicalButton{code, modifiers, edge, repeat}); };
    key(PhysicalKey::Tab, Pressed, shift);
    key(PhysicalKey::Tab, Pressed, {}, true);
    key(PhysicalKey::Tab, Released);
    key(PhysicalKey::Tab, Pressed, {}, true);
    key(PhysicalKey::GamepadDPadDown, Pressed);
    key(PhysicalKey::Down, Pressed);
    input_.repeat_navigation(dispatch_, 0.41);
    key(PhysicalKey::GamepadDPadDown, Released);
    input_.repeat_navigation(dispatch_, 0.41);
    input_.synchronize(dispatch_, {InputContext::Gameplay}, {});

    ASSERT_EQ(dispatch_.size(), 6u);
    auto const& transitions{input_.state().transitions};
    ASSERT_EQ(transitions.size(), 4u);
    std::size_t transition_index{};
    for (std::size_t i{}; i < dispatch_.size(); ++i) {
        auto const& button{std::get<host::MenuButton>(dispatch_.menu_input()[i])};
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
    key_event(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    key_event(SDLK_DOWN, SDL_SCANCODE_DOWN);
    key_event(SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);
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

TEST_F(LogicalPlayerInput, FocusLossClearsHeldActionsAndObserverMotion) {
    input_.synchronize(dispatch_, {InputContext::Observer}, {});
    map(PhysicalPointerButton{{}, PhysicalKey::RightMouseButton, ButtonState::Pressed, 1});
    map(PointerMotion{{}, {20.f, 10.f}});
    map(WheelMotion{{}, 1.f});
    map(PhysicalButton{PhysicalKey::D, {}, ButtonState::Pressed});
    EXPECT_TRUE(input_.state().held.observer_right());
    input_.append_frame_commands(0.016, dispatch_);
    ASSERT_FALSE(dispatch_.presentation_input().empty());
    EXPECT_FLOAT_EQ(std::get<host::ViewerStep>(dispatch_.presentation_input().back()).motion.speed,
                    150000.f);
    EXPECT_EQ(input_.state().axes_2d[Axis2DAction::ObserverLookDelta], (Vector2f{20.f, -10.f}));
    map(WindowFocus::Lost);
    EXPECT_FALSE(input_.relative_mouse());
    EXPECT_FALSE(input_.state().held.observer_right());
    EXPECT_EQ(input_.state().axes_2d[Axis2DAction::ObserverLookDelta], Vector2f{});

    map(WindowFocus::Gained);
    input_.synchronize(dispatch_, {InputContext::Menu}, {});
    dispatch_.begin_frame();
    map(PhysicalButton{PhysicalKey::GamepadDPadDown, {}, ButtonState::Pressed});
    input_.repeat_navigation(dispatch_, 0.41);
    ASSERT_EQ(dispatch_.size(), 2u);
    EXPECT_FALSE(std::get<host::MenuButton>(dispatch_.menu_input().front()).repeat);
    EXPECT_TRUE(std::get<host::MenuButton>(dispatch_.menu_input().back()).repeat);
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
    key_event(SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);
    focus(SDL_EVENT_WINDOW_FOCUS_GAINED);
    key_event(SDLK_S, SDL_SCANCODE_S);
    frame();
    EXPECT_TRUE(std::ranges::equal(ship_frame().events,
                                   std::array{sim::player::PlayerInputKind::BrakePressed}));
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

    settings_.bindings[FlightMapping::GeneralPauseKeyboardMouse] = PhysicalKey::MiddleMouseButton;
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

TEST_F(LogicalPlayerInput, HeldAxisNeedsANewActivationAcrossContextRebindAndFocus) {
    using enum sim::player::PlayerInputKind;
    auto route = [&](InputContext context) { input_.synchronize(dispatch_, {context}, {}); };
    auto sample = [&](float value) {
        map(PhysicalAxis{PhysicalKey::GamepadRightTriggerAxis, value});
    };
    route(InputContext::Gameplay);
    sample(0.50f);
    sample(0.45f);
    sample(0.51f);
    route(InputContext::Menu);
    route(InputContext::Gameplay);
    sample(0.9f);
    EXPECT_FALSE(input_.state().held.fire());
    sample(0.40f);
    sample(0.51f);
    input_.refresh_bindings(dispatch_);
    sample(0.7f);
    EXPECT_FALSE(input_.state().held.fire());
    sample(0.3f);
    sample(0.6f);
    map(WindowFocus::Lost);
    sample(0.7f);
    map(WindowFocus::Gained);
    sample(0.8f);
    EXPECT_FALSE(input_.state().held.fire());
    sample(0.39f);
    sample(0.6f);
    map(PhysicalPointerButton{{}, PhysicalKey::LeftMouseButton, ButtonState::Pressed, 1});
    map(GamepadDisconnected{});
    EXPECT_TRUE(input_.state().held.fire());
    map(PhysicalPointerButton{{}, PhysicalKey::LeftMouseButton, ButtonState::Released, 1});
    dispatch_.publish_ship(input_.state());
    EXPECT_TRUE(std::ranges::equal(dispatch_.ship_input().events,
                                   std::array{FirePressed,
                                              FireReleased,
                                              FirePressed,
                                              FireReleased,
                                              FirePressed,
                                              FireReleased,
                                              FirePressed,
                                              FireReleased}));
}

TEST_F(LogicalPlayerInput, RebindingFireToAnAlreadyHeldStickWaitsForNeutral) {
    input_.synchronize(dispatch_, {InputContext::Gameplay}, {});
    auto sample = [&](float value) { map(PhysicalAxis{PhysicalKey::GamepadRightX, value}); };
    sample(-0.8f);
    settings_.bindings[FlightMapping::FlightFirePrimaryGamepad] = PhysicalKey::GamepadRightX;
    input_.refresh_bindings(dispatch_);
    sample(-0.6f);
    EXPECT_TRUE(input_.state().transitions.empty());
    sample(-0.4f);
    sample(-0.5f);
    EXPECT_TRUE(input_.state().held.fire());
    sample(-0.45f);
    EXPECT_TRUE(input_.state().held.fire());
    sample(-0.4f);
    EXPECT_FALSE(input_.state().held.fire());
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
