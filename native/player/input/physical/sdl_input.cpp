#include "player/input/physical/sdl_input.h"

#include <ioj/input/dispatch/events.h>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <array>
#include <utility>

namespace ioj {
namespace {
void present_keyboard_layout() {
    std::array<host::KeyboardLabel, physical_key_keyboard_key_values.size()> labels;
    for (std::size_t i{}; i < labels.size(); ++i) {
        auto const key{physical_key_keyboard_key_values[i]};
        auto const scan{static_cast<SDL_Scancode>(key)};
        auto const logical{SDL_GetKeyFromScancode(scan, SDL_KMOD_NONE, false)};
        auto const* name{SDL_GetKeyName(logical)};
        labels[i] = {key, *name ? name : SDL_GetScancodeName(scan)};
    }
    host::present_keyboard_layout(labels);
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
    PhysicalKey::GamepadRightTriggerAxis,
};

}

SdlInput::SdlInput(SDL_Window& window)
    : window_{window} {
    present_keyboard_layout();
}
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
auto SdlInput::translate(SDL_Event const& event) -> std::optional<PhysicalEvent> {
    using enum ButtonState;
    if (event.type == SDL_EVENT_KEYMAP_CHANGED) {
        present_keyboard_layout();
        return {};
    }

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
            return GamepadDisconnected{};
        }
        return {};
    }
    auto const* target{SDL_GetWindowFromEvent(&event)};
    if (target && target != &window_) {
        return {};
    }
    switch (event.type) {
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            return WindowFocus::Lost;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            return WindowFocus::Gained;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            auto const key{static_cast<PhysicalKey>(event.key.scancode)};
            if (is_keyboard_key(key)) {
                return PhysicalButton{key,
                                      modifiers(event.key.mod),
                                      event.key.down ? Pressed : Released,
                                      event.key.repeat};
            }
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            return TextInput{event.text.text};
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            auto const& button{event.button};
            auto const code{mouse_key(button.button)};
            if (code != PhysicalKey::Unbound) {
                return PhysicalPointerButton{
                    {button.x, button.y}, code, button.down ? Pressed : Released, button.clicks};
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            return PointerMotion{{event.motion.x, event.motion.y},
                                 {event.motion.xrel, event.motion.yrel}};
        case SDL_EVENT_MOUSE_WHEEL:
            return WheelMotion{{event.wheel.mouse_x, event.wheel.mouse_y},
                               event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y
                                                                               : event.wheel.y};
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gbutton.which) {
                auto const code{pad_button(event.gbutton.button)};
                if (code != PhysicalKey::Unbound) {
                    return PhysicalButton{code, {}, event.gbutton.down ? Pressed : Released};
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gaxis.which) {
                auto const axis{event.gaxis.axis};
                return PhysicalAxis{
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
