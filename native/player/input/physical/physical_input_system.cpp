#include "player/input/physical/physical_input_system.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <utility>

namespace ioj {
namespace {
void refresh_keyboard_layout(KeyboardLayout& layout) {
    for (auto key : physical_key_keyboard_key_values) {
        auto const scan{static_cast<SDL_Scancode>(key)};
        auto const logical{SDL_GetKeyFromScancode(scan, SDL_KMOD_NONE, false)};
        auto const* name{SDL_GetKeyName(logical)};
        layout.names[std::to_underlying(key)] = *name ? name : SDL_GetScancodeName(scan);
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
    PhysicalKey::GamepadRightTriggerAxis,
};

}

auto PhysicalInputState::track_button(PhysicalKey key, ButtonState edge, bool repeat) -> bool {
    if (repeat) {
        return std::ranges::contains(held, key);
    }
    if (edge == ButtonState::Released) {
        return std::erase(held, key) != 0;
    } else if (edge == ButtonState::Pressed && !std::ranges::contains(held, key)) {
        held.push_back(key);
        return true;
    }
    return false;
}
auto PhysicalInputState::axis_edge(PhysicalKey key, float magnitude) -> std::optional<ButtonState> {
    auto const was_held{std::ranges::contains(held, key)};
    auto const active{was_held ? magnitude > 0.40f : magnitude >= 0.50f};
    if (was_held == active) {
        return {};
    }
    auto const edge{active ? ButtonState::Pressed : ButtonState::Released};
    track_button(key, edge);
    return edge;
}

PhysicalInputSystem::PhysicalInputSystem(SDL_Window& window, KeyboardLayout& layout)
    : window_{window}
    , layout_{layout} {
    state_.focus = focused() ? WindowFocus::Gained : WindowFocus::Lost;
    refresh_keyboard_layout(layout_);
}
PhysicalInputSystem::~PhysicalInputSystem() {
    SDL_CloseGamepad(gamepad_);
}
auto PhysicalInputSystem::focused() const -> bool {
    return SDL_GetKeyboardFocus() == &window_;
}
void PhysicalInputSystem::set_window_modes(bool relative_mouse, bool text_input) {
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
auto PhysicalInputSystem::poll() -> WindowInputEvents {
    events_.clear();
    WindowInputEvents window;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                window.close_requested = true;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_MOVED:
                window.viewport_changed = true;
                break;
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
            case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
                window.display_changed = true;
                break;
            default:
                collect(event);
                break;
        }
    }
    return window;
}
void PhysicalInputSystem::collect(SDL_Event const& event) {
    using enum ButtonState;
    if (event.type == SDL_EVENT_KEYMAP_CHANGED) {
        refresh_keyboard_layout(layout_);
        return;
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
            events_.order.push_back({PhysicalEventKind::Disconnect, 0});
        }
        return;
    }
    auto const* target{SDL_GetWindowFromEvent(&event)};
    if (target && target != &window_) {
        return;
    }
    switch (event.type) {
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            events_.order.push_back({PhysicalEventKind::FocusLost, 0});
            return;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            events_.order.push_back({PhysicalEventKind::FocusGained, 0});
            return;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            auto const key{static_cast<PhysicalKey>(event.key.scancode)};
            if (is_keyboard_key(key)) {
                events_.button(key,
                               modifiers(event.key.mod),
                               event.key.down ? Pressed : Released,
                               event.key.repeat);
            }
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            events_.order.push_back({PhysicalEventKind::Text, events_.text.size()});
            events_.text.emplace_back(event.text.text);
            return;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            auto const& button{event.button};
            auto const code{mouse_key(button.button)};
            if (code != PhysicalKey::Unbound) {
                events_.pointer_button(
                    {button.x, button.y}, code, button.down ? Pressed : Released, button.clicks);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            events_.motion({event.motion.x, event.motion.y},
                           {event.motion.xrel, event.motion.yrel});
            return;
        case SDL_EVENT_MOUSE_WHEEL:
            events_.wheel({event.wheel.mouse_x, event.wheel.mouse_y},
                          event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y
                                                                          : event.wheel.y);
            return;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gbutton.which) {
                auto const code{pad_button(event.gbutton.button)};
                if (code != PhysicalKey::Unbound) {
                    events_.button(code, {}, event.gbutton.down ? Pressed : Released);
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gaxis.which) {
                auto const axis{event.gaxis.axis};
                events_.axis(axis_keys[axis],
                             axis_value(static_cast<SDL_GamepadAxis>(axis), event.gaxis.value));
            }
            break;
        default:
            break;
    }
}
}
