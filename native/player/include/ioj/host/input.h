#pragma once

#include <ioj/host/modifiers.h>
#include <ioj/host/pause_menu.h>
#include <ioj/input/key_code.h>
#include <ioj/input/menu_navigation.h>
#include <ioj/vector.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ioj::host {
struct MenuStatus {
    bool editing_text{};
    bool capturing_binding{};
    std::optional<MainMenuItem> main_focus;
};

struct PointerInput {
    PointerAction action;
    PhysicalKey button;
    float x;
    float y;
    float wheel;
};

struct ViewerMotion {
    float right;
    float forward;
    float up;
    float yaw;
    float pitch;
    float speed;
    bool looking;
};

struct MenuButton {
    MenuCommand command;
    bool down;
    bool repeat;
    Modifiers modifiers;
};
struct MenuKey {
    KeyCode key;
    std::uint32_t character{};
    bool down{};
    bool repeat{};
    Modifiers modifiers{};
};
struct MenuAxis {
    KeyCode key;
    float value;
    Modifiers modifiers;
};
struct MenuPointer {
    PointerInput pointer;
    Modifiers modifiers;
};
struct MenuText {
    std::string text;
    Modifiers modifiers;
};
struct SteeringIndicator {
    std::optional<Vector2d> direction;
};
struct ViewerStep {
    ViewerMotion motion;
    double seconds;
};
using PresentationEvent =
    std::variant<MenuKey, MenuAxis, MenuPointer, MenuText, SteeringIndicator, ViewerStep>;
using WidgetAction = std::variant<PlayerAction, MainMenuItem, PauseWidgetEvent>;
using InputEvent = std::variant<PlayerAction, MenuButton, PresentationEvent>;
using InputEvents = std::vector<InputEvent>;

#ifdef IOJ_PLAYER_EXPORTS
#define IOJ_INPUT_API __declspec(dllexport)
#else
#define IOJ_INPUT_API __declspec(dllimport)
#endif
IOJ_INPUT_API auto menu_status() -> MenuStatus;
IOJ_INPUT_API auto present_pause(PauseCommand command) -> bool;
IOJ_INPUT_API void select_benchmark_button(bool end_selected);
IOJ_INPUT_API void focus_main_item(MainMenuItem item);
IOJ_INPUT_API void activate_main_item(MainMenuItem item);
IOJ_INPUT_API void present_menu_button(MenuButton button);
IOJ_INPUT_API auto take_widget_actions() -> std::span<WidgetAction const>;
IOJ_INPUT_API void present_input(PresentationEvent const& event);
#undef IOJ_INPUT_API
}
