#include "player/sdl_input.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <array>
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
auto modifiers(SDL_Keymod mod) -> host::Modifiers {
    return host::Modifiers{bool(mod & SDL_KMOD_LSHIFT),
                           bool(mod & SDL_KMOD_RSHIFT),
                           bool(mod & SDL_KMOD_LCTRL),
                           bool(mod & SDL_KMOD_RCTRL),
                           bool(mod & SDL_KMOD_LALT),
                           bool(mod & SDL_KMOD_RALT),
                           bool(mod & SDL_KMOD_LGUI),
                           bool(mod & SDL_KMOD_RGUI),
                           bool(mod & SDL_KMOD_CAPS)};
}
auto axis_value(SDL_GamepadAxis axis, Sint16 sample) -> float {
    auto const value{sample / (sample < 0 ? 32768.f : 32767.f)};
    return axis == SDL_GAMEPAD_AXIS_LEFTY || axis == SDL_GAMEPAD_AXIS_RIGHTY ? -value : value;
}
constexpr std::array<PhysicalKey, SDL_GAMEPAD_AXIS_COUNT> axis_keys{
    PhysicalKey::GamepadLeftX,
    PhysicalKey::GamepadLeftY,
    PhysicalKey::GamepadRightX,
    PhysicalKey::GamepadRightY,
    PhysicalKey::GamepadLeftTriggerAxis,
    PhysicalKey::GamepadRightTriggerAxis};

}

SdlInput::SdlInput(SDL_Window& window)
    : window_{window} {}
SdlInput::~SdlInput() {
    SDL_CloseGamepad(gamepad_);
}
auto SdlInput::focused() const -> bool {
    return SDL_GetKeyboardFocus() == &window_;
}
void SdlInput::set_window_modes(bool relative_mouse, bool text_input) {
    auto const focus{focused()};
    auto const relative{relative_mouse && focus};
    if (SDL_GetWindowRelativeMouseMode(&window_) != relative) {
        SDL_SetWindowRelativeMouseMode(&window_, relative);
    }
    auto const editing{text_input && focus};
    if (SDL_TextInputActive(&window_) != editing) {
        if (editing) {
            SDL_StartTextInput(&window_);
        } else {
            SDL_StopTextInput(&window_);
        }
    }
}
auto SdlInput::translate(SDL_Event const& event) -> std::optional<input::PhysicalEvent> {
    using enum input::ButtonState;
    if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        bool const removed{gamepad_ && event.type == SDL_EVENT_GAMEPAD_REMOVED &&
                           SDL_GetGamepadID(gamepad_) == event.gdevice.which};
        if (removed) {
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

        if (removed) {
            return input::GamepadDisconnected{};
        }
        return {};
    }
    auto const* target{SDL_GetWindowFromEvent(&event)};
    if (target && target != &window_) {
        return {};
    }
    switch (event.type) {
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            return input::WindowFocus::Lost;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            return input::WindowFocus::Gained;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            return input::PhysicalButton{keyboard_key(event.key),
                                         event.key.key,
                                         modifiers(event.key.mod),
                                         event.key.repeat ? Repeated
                                         : event.key.down ? Pressed
                                                          : Released};
        }
        case SDL_EVENT_TEXT_INPUT:
            return input::TextInput{event.text.text};
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            auto const& button{event.button};
            auto const code{mouse_key(button.button)};
            if (code != PhysicalKey::Unbound) {
                return input::PhysicalPointerButton{
                    {button.x, button.y}, code, button.down ? Pressed : Released, button.clicks};
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            return input::PointerMotion{{event.motion.x, event.motion.y},
                                        {event.motion.xrel, event.motion.yrel}};
        case SDL_EVENT_MOUSE_WHEEL:
            return input::WheelMotion{
                {event.wheel.mouse_x, event.wheel.mouse_y},
                event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y};
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gbutton.which) {
                auto const code{pad_button(event.gbutton.button)};
                if (code != PhysicalKey::Unbound) {
                    return input::PhysicalButton{
                        code, 0, {}, event.gbutton.down ? Pressed : Released};
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gaxis.which) {
                auto const axis{event.gaxis.axis};
                return input::PhysicalAxis{
                    axis_keys[axis],
                    axis_value(static_cast<SDL_GamepadAxis>(axis), event.gaxis.value)};
            }
            break;
        default:
            break;
    }
    return {};
}
}
