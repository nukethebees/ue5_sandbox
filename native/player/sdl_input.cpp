#include "player/sdl_input.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace ioj {
namespace {
auto keyboard_key(SDL_KeyboardEvent const& key) -> input::KeyCode {
    static constexpr PhysicalKey keypad[]{PhysicalKey::NumPadOne,
                                          PhysicalKey::NumPadTwo,
                                          PhysicalKey::NumPadThree,
                                          PhysicalKey::NumPadFour,
                                          PhysicalKey::NumPadFive,
                                          PhysicalKey::NumPadSix,
                                          PhysicalKey::NumPadSeven,
                                          PhysicalKey::NumPadEight,
                                          PhysicalKey::NumPadNine,
                                          PhysicalKey::NumPadZero};
    if (key.scancode >= SDL_SCANCODE_KP_1 && key.scancode <= SDL_SCANCODE_KP_0) {
        return keypad[key.scancode - SDL_SCANCODE_KP_1];
    }
    if (key.scancode >= SDL_SCANCODE_F1 && key.scancode <= SDL_SCANCODE_F12) {
        return static_cast<PhysicalKey>(std::to_underlying(PhysicalKey::F1) + key.scancode -
                                        SDL_SCANCODE_F1);
    }
    switch (key.scancode) {
        case SDL_SCANCODE_RETURN:
            return PhysicalKey::Enter;
        case SDL_SCANCODE_ESCAPE:
            return PhysicalKey::Escape;
        case SDL_SCANCODE_BACKSPACE:
            return PhysicalKey::BackSpace;
        case SDL_SCANCODE_TAB:
            return PhysicalKey::Tab;
        case SDL_SCANCODE_SPACE:
            return PhysicalKey::SpaceBar;
        case SDL_SCANCODE_CAPSLOCK:
            return PhysicalKey::CapsLock;
        case SDL_SCANCODE_PRINTSCREEN:
            return PhysicalKey::PrintScreen;
        case SDL_SCANCODE_SCROLLLOCK:
            return PhysicalKey::ScrollLock;
        case SDL_SCANCODE_PAUSE:
            return PhysicalKey::Pause;
        case SDL_SCANCODE_INSERT:
            return PhysicalKey::Insert;
        case SDL_SCANCODE_HOME:
            return PhysicalKey::Home;
        case SDL_SCANCODE_PAGEUP:
            return PhysicalKey::PageUp;
        case SDL_SCANCODE_DELETE:
            return PhysicalKey::Delete;
        case SDL_SCANCODE_END:
            return PhysicalKey::End;
        case SDL_SCANCODE_PAGEDOWN:
            return PhysicalKey::PageDown;
        case SDL_SCANCODE_RIGHT:
            return PhysicalKey::Right;
        case SDL_SCANCODE_LEFT:
            return PhysicalKey::Left;
        case SDL_SCANCODE_DOWN:
            return PhysicalKey::Down;
        case SDL_SCANCODE_UP:
            return PhysicalKey::Up;
        case SDL_SCANCODE_NUMLOCKCLEAR:
            return PhysicalKey::NumLock;
        case SDL_SCANCODE_KP_DIVIDE:
            return PhysicalKey::Divide;
        case SDL_SCANCODE_KP_MULTIPLY:
            return PhysicalKey::Multiply;
        case SDL_SCANCODE_KP_MINUS:
            return PhysicalKey::Subtract;
        case SDL_SCANCODE_KP_PLUS:
            return PhysicalKey::Add;
        case SDL_SCANCODE_KP_ENTER:
            return PhysicalKey::Enter;
        case SDL_SCANCODE_KP_PERIOD:
            return PhysicalKey::Decimal;
        case SDL_SCANCODE_LCTRL:
            return PhysicalKey::LeftControl;
        case SDL_SCANCODE_LSHIFT:
            return PhysicalKey::LeftShift;
        case SDL_SCANCODE_LALT:
            return PhysicalKey::LeftAlt;
        case SDL_SCANCODE_LGUI:
            return PhysicalKey::LeftCommand;
        case SDL_SCANCODE_RCTRL:
            return PhysicalKey::RightControl;
        case SDL_SCANCODE_RSHIFT:
            return PhysicalKey::RightShift;
        case SDL_SCANCODE_RALT:
            return PhysicalKey::RightAlt;
        case SDL_SCANCODE_RGUI:
            return PhysicalKey::RightCommand;
        case SDL_SCANCODE_MEDIA_NEXT_TRACK:
            return PhysicalKey::MediaNext;
        case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK:
            return PhysicalKey::MediaPrevious;
        case SDL_SCANCODE_MEDIA_PLAY_PAUSE:
            return PhysicalKey::MediaPlayPause;
        case SDL_SCANCODE_MEDIA_STOP:
            return PhysicalKey::MediaStop;
        case SDL_SCANCODE_VOLUMEUP:
            return PhysicalKey::VolumeUp;
        case SDL_SCANCODE_VOLUMEDOWN:
            return PhysicalKey::VolumeDown;
        case SDL_SCANCODE_MUTE:
            return PhysicalKey::VolumeMute;
        default:
            return key.key >= 32 && key.key <= 0x10ffff ? input::character_key(key.key)
                                                        : input::KeyCode{};
    }
}
auto pad_button(std::size_t button) -> PhysicalKey {
    switch (button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return PhysicalKey::GamepadFaceButtonBottom;
        case SDL_GAMEPAD_BUTTON_EAST:
            return PhysicalKey::GamepadFaceButtonRight;
        case SDL_GAMEPAD_BUTTON_NORTH:
            return PhysicalKey::GamepadFaceButtonTop;
        case SDL_GAMEPAD_BUTTON_WEST:
            return PhysicalKey::GamepadFaceButtonLeft;
        case SDL_GAMEPAD_BUTTON_BACK:
            return PhysicalKey::GamepadSpecialLeft;
        case SDL_GAMEPAD_BUTTON_START:
            return PhysicalKey::GamepadSpecialRight;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            return PhysicalKey::GamepadLeftThumbstick;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            return PhysicalKey::GamepadRightThumbstick;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return PhysicalKey::GamepadLeftShoulder;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return PhysicalKey::GamepadRightShoulder;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return PhysicalKey::GamepadDPadUp;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return PhysicalKey::GamepadDPadDown;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return PhysicalKey::GamepadDPadLeft;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return PhysicalKey::GamepadDPadRight;
        default:
            return {};
    }
}
auto mouse_key(Uint8 button) -> PhysicalKey {
    switch (button) {
        case SDL_BUTTON_LEFT:
            return PhysicalKey::LeftMouseButton;
        case SDL_BUTTON_RIGHT:
            return PhysicalKey::RightMouseButton;
        case SDL_BUTTON_MIDDLE:
            return PhysicalKey::MiddleMouseButton;
        case SDL_BUTTON_X1:
            return PhysicalKey::ThumbMouseButton;
        case SDL_BUTTON_X2:
            return PhysicalKey::ThumbMouseButton2;
        default:
            return {};
    }
}
constexpr std::array<PhysicalKey, SDL_GAMEPAD_AXIS_COUNT> axis_keys{
    PhysicalKey::GamepadLeftX,
    PhysicalKey::GamepadLeftY,
    PhysicalKey::GamepadRightX,
    PhysicalKey::GamepadRightY,
    PhysicalKey::GamepadLeftTriggerAxis,
    PhysicalKey::GamepadRightTriggerAxis};
constexpr std::array<PhysicalKey, 6> positive_keys{PhysicalKey::GamepadLeftStickRight,
                                                   PhysicalKey::GamepadLeftStickUp,
                                                   PhysicalKey::GamepadRightStickRight,
                                                   PhysicalKey::GamepadRightStickUp,
                                                   PhysicalKey::GamepadLeftTrigger,
                                                   PhysicalKey::GamepadRightTrigger};
constexpr std::array<PhysicalKey, 4> negative_keys{PhysicalKey::GamepadLeftStickLeft,
                                                   PhysicalKey::GamepadLeftStickDown,
                                                   PhysicalKey::GamepadRightStickLeft,
                                                   PhysicalKey::GamepadRightStickDown};
constexpr std::array<float, 5> viewer_speeds{5000.f, 15000.f, 50000.f, 150000.f, 500000.f};
}

SdlInput::SdlInput(SDL_Window& window, ControlsSettings const& controls)
    : window_{window}
    , controls_{controls} {}
SdlInput::~SdlInput() {
    SDL_CloseGamepad(gamepad_);
}
void SdlInput::begin_frame() {
    events_.clear();
    flight_.begin_frame();
}
void SdlInput::synchronize(host::InputRoute current, host::MenuStatus menu) {
    if (route_ != current) {
        viewer_buttons_ = {};
        viewer_looking_ = false;
        viewer_dx_ = 0.f;
        viewer_dy_ = 0.f;
        dpad_repeat_ = {};
        route_ = current;
        refresh_bindings();
    }
    bool const focused{SDL_GetKeyboardFocus() == &window_};
    auto const relative{focused &&
                        (current.context == input::Context::Gameplay || viewer_looking_)};
    if (SDL_GetWindowRelativeMouseMode(&window_) != relative) {
        SDL_SetWindowRelativeMouseMode(&window_, relative);
    }
    bool const editing{focused && current.context == input::Context::Menu && menu.editing_text};
    if (SDL_TextInputActive(&window_) != editing) {
        if (editing) {
            SDL_StartTextInput(&window_);
        } else {
            SDL_StopTextInput(&window_);
        }
    }
}
void SdlInput::refresh_bindings() {
    flight_.configure(controls_, route_.context);
    if (route_.context == input::Context::Gameplay) {
        for (std::size_t axis{}; axis < axes_.size(); ++axis) {
            float const position{axes_[axis]};
            flight_.restore_axis(axis_keys[axis], position, controls_);
            flight_.restore_axis(positive_keys[axis], position >= 0.5f ? 1.f : 0.f, controls_);
            if (axis < negative_keys.size()) {
                flight_.restore_axis(negative_keys[axis], position <= -0.5f ? 1.f : 0.f, controls_);
            }
        }
    }
}
void SdlInput::key(input::KeyCode code, bool down, bool repeat, Uint32 character) {
    if (route_.context == input::Context::Menu) {
        events_.emplace_back(host::MenuKey{code, character, down, repeat, modifiers_});
        return;
    }
    if (repeat || code == PhysicalKey::Unbound) {
        return;
    }
    if (route_.context == input::Context::Gameplay) {
        if (flight_.update(code, down ? 1.f : 0.f, controls_)) {
            host::pause_or_exit_viewer();
        }
        return;
    }

    if (down && ((!route_.movable_viewer && code == PhysicalKey::Escape) ||
                 (route_.movable_viewer && flight_.update(code, 1.f, controls_)))) {
        host::pause_or_exit_viewer();
        return;
    }
    if (!route_.movable_viewer) {
        return;
    }
    if (!down) {
        flight_.update(code, 0.f, controls_);
    }
    if (code == PhysicalKey::RightMouseButton && down) {
        viewer_looking_ = !viewer_looking_;
    }
    static constexpr std::array<PhysicalKey, 7> movement_keys{PhysicalKey::D,
                                                              PhysicalKey::A,
                                                              PhysicalKey::W,
                                                              PhysicalKey::S,
                                                              PhysicalKey::E,
                                                              PhysicalKey::Q,
                                                              PhysicalKey::LeftShift};
    auto const found{std::ranges::find(movement_keys, code.key)};
    if (found != movement_keys.end()) {
        viewer_buttons_[static_cast<std::size_t>(found - movement_keys.begin())] = down;
    }
}
void SdlInput::handle(SDL_Event const& event) {
    events_.clear();
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
            key(keyboard_key(event.key), event.key.down, event.key.repeat, event.key.key);
            if (event.key.down && event.key.key == SDLK_BACKSPACE &&
                route_.context == input::Context::Menu && !(mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) &&
                SDL_TextInputActive(&window_)) {
                events_.emplace_back(host::MenuText{"\b", modifiers_});
            }
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (route_.context == input::Context::Menu) {
                events_.emplace_back(host::MenuText{event.text.text, modifiers_});
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            auto const& button{event.button};
            auto const name{mouse_key(button.button)};
            if (name == PhysicalKey::Unbound) {
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
                events_.emplace_back(host::MenuPointer{{action, name, button.x, button.y, 0.f}, modifiers_});
            } else if (!button.down && (ui_mouse_buttons_ & mask)) {
                ui_mouse_buttons_ &= ~mask;
                events_.emplace_back(host::MenuPointer{{host::PointerAction::Up, name, button.x, button.y, 0.f}, modifiers_});
            }
            if (route_.context != input::Context::Menu) {
                key(name, button.down, false, 0);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            if (route_.context == input::Context::Gameplay) {
                flight_.update(PhysicalKey::MouseX, event.motion.xrel, controls_);
                flight_.update(PhysicalKey::MouseY, event.motion.yrel, controls_);
            } else if (viewer_looking_) {
                viewer_dx_ += event.motion.xrel;
                viewer_dy_ -= event.motion.yrel;
            } else {
                events_.emplace_back(host::MenuPointer{{host::PointerAction::Move, PhysicalKey::Unbound, event.motion.x, event.motion.y, 0.f}, modifiers_});
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            auto const amount{event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y
                                                                              : event.wheel.y};
            if (route_.context == input::Context::Menu) {
                events_.emplace_back(host::MenuPointer{{host::PointerAction::Wheel,
                                  PhysicalKey::Unbound,
                                  event.wheel.mouse_x,
                                  event.wheel.mouse_y,
                                  amount}, modifiers_});
            } else if (route_.movable_viewer) {
                speed_index_ = std::clamp(speed_index_ + (amount > 0.f) - (amount < 0.f), 0, 4);
            } else if (route_.context == input::Context::Gameplay && amount != 0.f) {
                auto const name{amount > 0.f ? PhysicalKey::MouseScrollUp : PhysicalKey::MouseScrollDown};
                key(name, true, false, 0);
                key(name, false, false, 0);
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gbutton.which) {
                auto const name{pad_button(event.gbutton.button)};
                if (name != PhysicalKey::Unbound) {
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
        events_.emplace_back(host::MenuAxis{axis_keys[axis], value, modifiers_});
    } else if ((route_.context == input::Context::Gameplay || route_.movable_viewer) &&
               flight_.update(axis_keys[axis], value, controls_)) {
        host::pause_or_exit_viewer();
    }
    // Digital aliases are independently rebindable in the Controls menu.
    if ((before >= 0.5f) != (value >= 0.5f)) {
        key(positive_keys[axis], value >= 0.5f, false, 0);
    }
    if (axis < negative_keys.size() && (before <= -0.5f) != (value <= -0.5f)) {
        key(negative_keys[axis], value <= -0.5f, false, 0);
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
    events_.clear();
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
                events_.emplace_back(host::MenuAxis{axis_keys[axis], axes_[axis], modifiers_});
            }
        }
    }
    if (route_.context == input::Context::Gameplay) {
        events_.emplace_back(host::SteeringIndicator{flight_.steering()});
    }
    if (route_.movable_viewer) {
        auto const speed{viewer_speeds[speed_index_] * (viewer_buttons_[6] ? 4.f : 1.f)};
        events_.emplace_back(host::ViewerStep{{float(viewer_buttons_[0] - viewer_buttons_[1]),
                                               float(viewer_buttons_[2] - viewer_buttons_[3]),
                                               float(viewer_buttons_[4] - viewer_buttons_[5]),
                                               viewer_dx_ * 0.1f,
                                               viewer_dy_ * 0.1f,
                                               speed,
                                               viewer_looking_},
                                              dt});
    }
    viewer_dx_ = 0.f;
    viewer_dy_ = 0.f;
}
}
