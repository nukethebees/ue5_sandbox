#pragma once

#include <ioj/input/dispatch/events.h>
#include <ioj/input/dispatch/player_menu.h>

#include <optional>
#include <span>
#include <string>
#include <variant>

namespace ioj::host {
struct SettingsSaveResult {
    bool succeeded;
};
using WidgetAction = std::variant<PlayerAction, SettingsSaveResult>;
#ifdef IOJ_PLAYER_EXPORTS
#define IOJ_INPUT_API __declspec(dllexport)
#else
#define IOJ_INPUT_API __declspec(dllimport)
#endif
IOJ_INPUT_API auto present_pause(PauseCommand command) -> bool;
IOJ_INPUT_API void select_benchmark_button(bool end_selected);
IOJ_INPUT_API void present_menu(MenuUpdate const& update);
IOJ_INPUT_API auto take_widget_actions() -> std::span<WidgetAction const>;

#undef IOJ_INPUT_API
}
