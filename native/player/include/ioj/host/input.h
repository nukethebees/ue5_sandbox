#pragma once

#include <ioj/host/pause_menu.h>
#include <ioj/input/dispatch/events.h>

#include <optional>
#include <span>
#include <string>
#include <variant>

namespace ioj::host {
struct MenuStatus {
    MenuInputMode input_mode{MenuInputMode::Navigation};
    std::optional<MainMenuItem> main_focus;
};

using WidgetAction = std::variant<PlayerAction, MainMenuItem, PauseWidgetEvent>;
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
