#include "player/sdl_input.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

namespace ioj {
namespace {
auto key_name(SDL_KeyboardEvent const& key) -> std::string {
    static constexpr char const* digits[]{
        "Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine"};
    static constexpr char const* keypad[]{"NumPadOne",
                                          "NumPadTwo",
                                          "NumPadThree",
                                          "NumPadFour",
                                          "NumPadFive",
                                          "NumPadSix",
                                          "NumPadSeven",
                                          "NumPadEight",
                                          "NumPadNine",
                                          "NumPadZero"};
    if (key.key >= SDLK_A && key.key <= SDLK_Z) {
        return std::string(1, static_cast<char>('A' + key.key - SDLK_A));
    }
    if (key.key >= SDLK_0 && key.key <= SDLK_9) {
        return digits[key.key - SDLK_0];
    }
    if (key.scancode >= SDL_SCANCODE_KP_1 && key.scancode <= SDL_SCANCODE_KP_0) {
        return keypad[key.scancode - SDL_SCANCODE_KP_1];
    }
    if (key.scancode >= SDL_SCANCODE_F1 && key.scancode <= SDL_SCANCODE_F12) {
        return "F" + std::to_string(key.scancode - SDL_SCANCODE_F1 + 1);
    }
    switch (key.key) {
        case SDLK_MINUS:
            return "Hyphen";
        case SDLK_EQUALS:
            return "Equals";
        case SDLK_LEFTBRACKET:
            return "LeftBracket";
        case SDLK_RIGHTBRACKET:
            return "RightBracket";
        case SDLK_BACKSLASH:
            return "Backslash";
        case SDLK_SEMICOLON:
            return "Semicolon";
        case SDLK_APOSTROPHE:
            return "Apostrophe";
        case SDLK_GRAVE:
            return "Tilde";
        case SDLK_COMMA:
            return "Comma";
        case SDLK_PERIOD:
            return "Period";
        case SDLK_SLASH:
            return "Slash";
        case SDLK_UNDERSCORE:
            return "Underscore";
        case SDLK_AMPERSAND:
            return "Ampersand";
        case SDLK_ASTERISK:
            return "Asterix";
        case SDLK_CARET:
            return "Caret";
        case SDLK_COLON:
            return "Colon";
        case SDLK_DOLLAR:
            return "Dollar";
        case SDLK_EXCLAIM:
            return "Exclamation";
        case SDLK_LEFTPAREN:
            return "LeftParantheses";
        case SDLK_RIGHTPAREN:
            return "RightParantheses";
        case SDLK_DBLAPOSTROPHE:
            return "Quote";
        case 0xe0:
            return "A_AccentGrave";
        case 0xe8:
            return "E_AccentGrave";
        case 0xe9:
            return "E_AccentAigu";
        case 0xe7:
            return "C_Cedille";
        case 0xa7:
            return "Section";
        default:
            break;
    }
    switch (key.scancode) {
        case SDL_SCANCODE_RETURN:
            return "Enter";
        case SDL_SCANCODE_ESCAPE:
            return "Escape";
        case SDL_SCANCODE_BACKSPACE:
            return "BackSpace";
        case SDL_SCANCODE_TAB:
            return "Tab";
        case SDL_SCANCODE_SPACE:
            return "SpaceBar";
        case SDL_SCANCODE_CAPSLOCK:
            return "CapsLock";
        case SDL_SCANCODE_PRINTSCREEN:
            return "PrintScreen";
        case SDL_SCANCODE_SCROLLLOCK:
            return "ScrollLock";
        case SDL_SCANCODE_PAUSE:
            return "Pause";
        case SDL_SCANCODE_INSERT:
            return "Insert";
        case SDL_SCANCODE_HOME:
            return "Home";
        case SDL_SCANCODE_PAGEUP:
            return "PageUp";
        case SDL_SCANCODE_DELETE:
            return "Delete";
        case SDL_SCANCODE_END:
            return "End";
        case SDL_SCANCODE_PAGEDOWN:
            return "PageDown";
        case SDL_SCANCODE_RIGHT:
            return "Right";
        case SDL_SCANCODE_LEFT:
            return "Left";
        case SDL_SCANCODE_DOWN:
            return "Down";
        case SDL_SCANCODE_UP:
            return "Up";
        case SDL_SCANCODE_NUMLOCKCLEAR:
            return "NumLock";
        case SDL_SCANCODE_KP_DIVIDE:
            return "Divide";
        case SDL_SCANCODE_KP_MULTIPLY:
            return "Multiply";
        case SDL_SCANCODE_KP_MINUS:
            return "Subtract";
        case SDL_SCANCODE_KP_PLUS:
            return "Add";
        case SDL_SCANCODE_KP_ENTER:
            return "Enter";
        case SDL_SCANCODE_KP_PERIOD:
            return "Decimal";
        case SDL_SCANCODE_LCTRL:
            return "LeftControl";
        case SDL_SCANCODE_LSHIFT:
            return "LeftShift";
        case SDL_SCANCODE_LALT:
            return "LeftAlt";
        case SDL_SCANCODE_LGUI:
            return "LeftCommand";
        case SDL_SCANCODE_RCTRL:
            return "RightControl";
        case SDL_SCANCODE_RSHIFT:
            return "RightShift";
        case SDL_SCANCODE_RALT:
            return "RightAlt";
        case SDL_SCANCODE_RGUI:
            return "RightCommand";
        case SDL_SCANCODE_MEDIA_NEXT_TRACK:
            return "MediaNext";
        case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK:
            return "MediaPrevious";
        case SDL_SCANCODE_MEDIA_PLAY_PAUSE:
            return "MediaPlayPause";
        case SDL_SCANCODE_MEDIA_STOP:
            return "MediaStop";
        case SDL_SCANCODE_VOLUMEUP:
            return "VolumeUp";
        case SDL_SCANCODE_VOLUMEDOWN:
            return "VolumeDown";
        case SDL_SCANCODE_MUTE:
            return "VolumeMute";
        default:
            return key.key >= 32 && key.key <= 0x10ffff
                     ? "UnknownCharCode_" + std::to_string(key.key)
                     : std::string{};
    }
}
auto pad_button(std::size_t button) -> std::string_view {
    switch (button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return "Gamepad_FaceButton_Bottom";
        case SDL_GAMEPAD_BUTTON_EAST:
            return "Gamepad_FaceButton_Right";
        case SDL_GAMEPAD_BUTTON_NORTH:
            return "Gamepad_FaceButton_Top";
        case SDL_GAMEPAD_BUTTON_WEST:
            return "Gamepad_FaceButton_Left";
        case SDL_GAMEPAD_BUTTON_BACK:
            return "Gamepad_Special_Left";
        case SDL_GAMEPAD_BUTTON_START:
            return "Gamepad_Special_Right";
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            return "Gamepad_LeftThumbstick";
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            return "Gamepad_RightThumbstick";
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return "Gamepad_LeftShoulder";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return "Gamepad_RightShoulder";
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return "Gamepad_DPad_Up";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return "Gamepad_DPad_Down";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return "Gamepad_DPad_Left";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return "Gamepad_DPad_Right";
        default:
            return {};
    }
}
auto mouse_key(Uint8 button) -> std::string_view {
    switch (button) {
        case SDL_BUTTON_LEFT:
            return "LeftMouseButton";
        case SDL_BUTTON_RIGHT:
            return "RightMouseButton";
        case SDL_BUTTON_MIDDLE:
            return "MiddleMouseButton";
        case SDL_BUTTON_X1:
            return "ThumbMouseButton";
        case SDL_BUTTON_X2:
            return "ThumbMouseButton2";
        default:
            return {};
    }
}
constexpr std::array<std::string_view, SDL_GAMEPAD_AXIS_COUNT> axis_names{
    "Gamepad_LeftX",
    "Gamepad_LeftY",
    "Gamepad_RightX",
    "Gamepad_RightY",
    "Gamepad_LeftTriggerAxis",
    "Gamepad_RightTriggerAxis"};
constexpr std::array<float, 5> viewer_speeds{5000.f, 15000.f, 50000.f, 150000.f, 500000.f};
}

SdlInput::SdlInput(SDL_Window& window, ControlsSettings const& controls)
    : window_{window}
    , controls_{controls} {}
SdlInput::~SdlInput() {
    SDL_CloseGamepad(gamepad_);
}
void SdlInput::begin_frame() {
    flight_.begin_frame();
    synchronize();
}
void SdlInput::synchronize() {
    auto const current{host::input_route()};
    if (route_ != current) {
        viewer_buttons_ = {};
        viewer_looking_ = false;
        viewer_dx_ = 0.f;
        viewer_dy_ = 0.f;
        dpad_repeat_ = {};
        route_ = current;
        flight_.configure(controls_, current.context);
        if (current.context == input::Context::Gameplay) {
            for (std::size_t axis{}; axis < axes_.size(); ++axis) {
                flight_.restore_axis(axis_names[axis], axes_[axis], controls_);
            }
        }
    }
    bool const focused{SDL_GetKeyboardFocus() == &window_};
    auto const relative{focused &&
                        (current.context == input::Context::Gameplay || viewer_looking_)};
    if (SDL_GetWindowRelativeMouseMode(&window_) != relative) {
        SDL_SetWindowRelativeMouseMode(&window_, relative);
    }
    bool const editing{focused && current.context == input::Context::Menu && host::editing_text()};
    if (SDL_TextInputActive(&window_) != editing) {
        if (editing) {
            SDL_StartTextInput(&window_);
        } else {
            SDL_StopTextInput(&window_);
        }
    }
}
void SdlInput::key(std::string_view name, bool down, bool repeat, Uint32 character) {
    if (route_.context == input::Context::Menu) {
        host::ui_key(name, character, down, repeat, modifiers_);
        return;
    }
    if (repeat || name.empty()) {
        return;
    }
    if (route_.context == input::Context::Gameplay) {
        if (flight_.update(name, down ? 1.f : 0.f, controls_)) {
            host::pause_or_exit_viewer();
        }
        return;
    }

    if (down && ((!route_.movable_viewer && name == "Escape") ||
                 (route_.movable_viewer && flight_.update(name, 1.f, controls_)))) {
        host::pause_or_exit_viewer();
        return;
    }
    if (!route_.movable_viewer) {
        return;
    }
    if (!down) {
        flight_.update(name, 0.f, controls_);
    }
    if (name == "RightMouseButton" && down) {
        viewer_looking_ = !viewer_looking_;
    }
    static constexpr std::array<std::string_view, 7> movement_keys{
        "D", "A", "W", "S", "E", "Q", "LeftShift"};
    auto const found{std::ranges::find(movement_keys, name)};
    if (found != movement_keys.end()) {
        viewer_buttons_[static_cast<std::size_t>(found - movement_keys.begin())] = down;
    }
}
void SdlInput::handle(SDL_Event const& event) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        if (gamepad_ && event.type == SDL_EVENT_GAMEPAD_REMOVED &&
            SDL_GetGamepadID(gamepad_) == event.gdevice.which) {
            release_gamepad();
            SDL_CloseGamepad(gamepad_);
            gamepad_ = nullptr;
        }
        if (!gamepad_) {
            int count{};
            auto* const devices{SDL_GetGamepads(&count)};
            for (int i{}; i < count && !gamepad_; ++i) {
                gamepad_ = SDL_OpenGamepad(devices[i]);
            }
            SDL_free(devices);
        }
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        dpad_repeat_ = {};
        ui_mouse_buttons_ = 0;
        modifiers_ = {};
        release_gamepad();
        flight_.reset();
        viewer_buttons_ = {};
        viewer_looking_ = false;
        viewer_dx_ = 0.f;
        viewer_dy_ = 0.f;
        return;
    }

    if (SDL_GetKeyboardFocus() != &window_) {
        return;
    }
    synchronize();
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            auto const mod{event.key.mod};
            modifiers_ = host::Modifiers{bool(mod & SDL_KMOD_LSHIFT),
                                         bool(mod & SDL_KMOD_RSHIFT),
                                         bool(mod & SDL_KMOD_LCTRL),
                                         bool(mod & SDL_KMOD_RCTRL),
                                         bool(mod & SDL_KMOD_LALT),
                                         bool(mod & SDL_KMOD_RALT),
                                         bool(mod & SDL_KMOD_LGUI),
                                         bool(mod & SDL_KMOD_RGUI),
                                         bool(mod & SDL_KMOD_CAPS)};
            key(key_name(event.key), event.key.down, event.key.repeat, event.key.key);
            if (event.key.down && event.key.key == SDLK_BACKSPACE &&
                route_.context == input::Context::Menu && !(mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) &&
                host::editing_text()) {
                host::ui_text("\b", modifiers_);
            }
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (route_.context == input::Context::Menu) {
                host::ui_text(event.text.text, modifiers_);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            auto const& button{event.button};
            auto const name{mouse_key(button.button)};
            if (name.empty()) {
                break;
            }
            auto const mask{SDL_BUTTON_MASK(button.button)};
            bool const pointer_visible{
                route_.context == input::Context::Menu ||
                (route_.context == input::Context::BenchmarkViewer && !viewer_looking_)};
            bool const camera_toggle{route_.movable_viewer && button.button == SDL_BUTTON_RIGHT};
            if (button.down && pointer_visible && !camera_toggle) {
                ui_mouse_buttons_ |= mask;
                auto const action{button.clicks == 2 ? host::PointerAction::DoubleClick
                                                     : host::PointerAction::Down};
                host::ui_pointer({action, button.button, button.x, button.y, 0.f}, modifiers_);
            } else if (!button.down && (ui_mouse_buttons_ & mask)) {
                ui_mouse_buttons_ &= ~mask;
                host::ui_pointer({host::PointerAction::Up, button.button, button.x, button.y, 0.f},
                                 modifiers_);
            }
            if (route_.context != input::Context::Menu) {
                key(name, button.down, false, 0);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            if (route_.context == input::Context::Gameplay) {
                flight_.update("MouseX", event.motion.xrel, controls_);
                flight_.update("MouseY", event.motion.yrel, controls_);
            } else if (viewer_looking_) {
                viewer_dx_ += event.motion.xrel;
                viewer_dy_ -= event.motion.yrel;
            } else {
                host::ui_pointer(
                    {host::PointerAction::Move, 0, event.motion.x, event.motion.y, 0.f},
                    modifiers_);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            auto const amount{event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y
                                                                              : event.wheel.y};
            if (route_.context == input::Context::Menu) {
                host::ui_pointer({host::PointerAction::Wheel,
                                  0,
                                  event.wheel.mouse_x,
                                  event.wheel.mouse_y,
                                  amount},
                                 modifiers_);
            } else if (route_.movable_viewer) {
                speed_index_ = std::clamp(speed_index_ + (amount > 0.f) - (amount < 0.f), 0, 4);
            } else if (route_.context == input::Context::Gameplay && amount != 0.f) {
                auto const name{amount > 0.f ? "MouseScrollUp" : "MouseScrollDown"};
                key(name, true, false, 0);
                key(name, false, false, 0);
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gbutton.which) {
                auto const name{pad_button(event.gbutton.button)};
                if (!name.empty()) {
                    auto const button{event.gbutton.button};
                    pad_buttons_[button] = event.gbutton.down;
                    if (button >= SDL_GAMEPAD_BUTTON_DPAD_UP &&
                        button <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
                        dpad_repeat_[button - SDL_GAMEPAD_BUTTON_DPAD_UP] =
                            event.gbutton.down && route_.context == input::Context::Menu ? 0.4
                                                                                         : 0.0;
                    }
                    key(name, event.gbutton.down, false, 0);
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gaxis.which) {
                auto const axis{event.gaxis.axis};
                auto value{event.gaxis.value / (event.gaxis.value < 0 ? 32768.f : 32767.f)};
                if (axis == SDL_GAMEPAD_AXIS_LEFTY || axis == SDL_GAMEPAD_AXIS_RIGHTY) {
                    value = -value;
                }
                axis_value(axis, value);
            }
            break;
        default:
            break;
    }
}
void SdlInput::axis_value(std::size_t axis, float value) {
    auto const before{axes_[axis]};
    axes_[axis] = value;
    if (route_.context == input::Context::Menu) {
        host::ui_axis(axis_names[axis], value, modifiers_);
    } else if ((route_.context == input::Context::Gameplay || route_.movable_viewer) &&
               flight_.update(axis_names[axis], value, controls_)) {
        host::pause_or_exit_viewer();
    }
    // Digital aliases are independently rebindable in the Controls menu.
    static constexpr std::array<std::string_view, 6> positive{"Gamepad_LeftStick_Right",
                                                              "Gamepad_LeftStick_Up",
                                                              "Gamepad_RightStick_Right",
                                                              "Gamepad_RightStick_Up",
                                                              "Gamepad_LeftTrigger",
                                                              "Gamepad_RightTrigger"};
    static constexpr std::array<std::string_view, 4> negative{"Gamepad_LeftStick_Left",
                                                              "Gamepad_LeftStick_Down",
                                                              "Gamepad_RightStick_Left",
                                                              "Gamepad_RightStick_Down"};
    if ((before >= 0.5f) != (value >= 0.5f)) {
        key(positive[axis], value >= 0.5f, false, 0);
    }
    if (axis < negative.size() && (before <= -0.5f) != (value <= -0.5f)) {
        key(negative[axis], value <= -0.5f, false, 0);
    }
}
void SdlInput::release_gamepad() {
    dpad_repeat_ = {};
    for (std::size_t i{}; i < pad_buttons_.size(); ++i) {
        if (pad_buttons_[i]) {
            key(pad_button(i), false, false, 0);
            pad_buttons_[i] = false;
        }
    }
    for (std::size_t i{}; i < axes_.size(); ++i) {
        if (axes_[i] != 0.f) {
            axis_value(i, 0.f);
        }
    }
}
void SdlInput::finish_frame(double dt) {
    synchronize();
    if (route_.context == input::Context::Menu && SDL_GetKeyboardFocus() == &window_) {
        for (std::size_t i{}; i < dpad_repeat_.size(); ++i) {
            auto& delay{dpad_repeat_[i]};
            if (delay > 0.0) {
                delay -= dt;
                if (delay <= 0.0) {
                    key(pad_button(SDL_GAMEPAD_BUTTON_DPAD_UP + i), true, true, 0);
                    delay = 0.1;
                }
            }
        }
        // Slate applies its own repeat interval to held analog navigation.
        for (auto axis : {SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY}) {
            if (std::abs(axes_[axis]) >= 0.5f) {
                host::ui_axis(axis_names[axis], axes_[axis], modifiers_);
            }
        }
    }
    if (route_.context == input::Context::Gameplay) {
        host::present_steering(flight_.steering());
    }
    if (route_.movable_viewer) {
        auto const speed{viewer_speeds[speed_index_] * (viewer_buttons_[6] ? 4.f : 1.f)};
        host::present_viewer_motion({float(viewer_buttons_[0] - viewer_buttons_[1]),
                                     float(viewer_buttons_[2] - viewer_buttons_[3]),
                                     float(viewer_buttons_[4] - viewer_buttons_[5]),
                                     viewer_dx_ * 0.1f,
                                     viewer_dy_ * 0.1f,
                                     speed,
                                     viewer_looking_},
                                    dt);
    }
    viewer_dx_ = 0.f;
    viewer_dy_ = 0.f;
}
}
